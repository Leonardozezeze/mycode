#include "sessionevents.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtNumeric>

namespace
{
constexpr qsizetype maxEvents = 100000;
constexpr qint64 maxLineBytes = 32 * 1024;
}

QByteArray SessionEvents::header() const
{
    return QJsonDocument(QJsonObject{{"format", "serial-wave-events"}, {"version", 1},
        {"started_at_utc", m_startedAt.toUTC().toString(Qt::ISODateWithMs)}})
        .toJson(QJsonDocument::Compact) + '\n';
}

QByteArray SessionEvents::line(const SessionEvent &event)
{
    return QJsonDocument(QJsonObject{{"time_s", event.time}, {"type", event.type},
                                     {"message", event.message}})
        .toJson(QJsonDocument::Compact) + '\n';
}

void SessionEvents::begin(const QString &session, const QDateTime &startedAt)
{
    m_path = fileName(session);
    m_startedAt = startedAt;
    m_events.clear();
    m_writtenCount = m_writtenBytes = 0;
    m_readOnly = false;
    m_ignoredTail = false;
    m_error.clear();
}

bool SessionEvents::load(const QString &session, const QDateTime &startedAt)
{
    begin(session, startedAt);
    m_readOnly = true;
    if (!QFile::exists(m_path))
        return true; // 兼容旧会话及单独复制的波形文件
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_error = file.errorString();
        return false;
    }
    auto fail = [this](const QString &message)
    {
        m_error = message;
        m_events.clear(); // 不能把身份不明或损坏文件里的部分事件当成完整记录
        return false;
    };
    const QByteArray first = file.readLine(maxLineBytes);
    QJsonParseError parseError;
    const auto meta = QJsonDocument::fromJson(first, &parseError).object();
    if (!first.endsWith('\n') || parseError.error != QJsonParseError::NoError ||
        meta.value("format").toString() != "serial-wave-events" ||
        meta.value("version").toInt() != 1 ||
        QDateTime::fromString(meta.value("started_at_utc").toString(), Qt::ISODateWithMs) != startedAt)
        return fail(QStringLiteral("事件文件格式或会话起点不匹配"));
    while (!file.atEnd())
    {
        const auto bytes = file.readLine(maxLineBytes);
        if (!bytes.endsWith('\n'))
        {
            if (!file.atEnd())
                return fail(QStringLiteral("事件文件包含过长记录"));
            m_ignoredTail = true;
            break;
        }
        const auto object = QJsonDocument::fromJson(bytes, &parseError).object();
        const auto time = object.value("time_s");
        const double value = time.toDouble(-1);
        const QString type = object.value("type").toString();
        const auto message = object.value("message");
        if (parseError.error != QJsonParseError::NoError || !time.isDouble() ||
            !qIsFinite(value) || value < 0 || type.isEmpty() || type.size() > 32 ||
            !message.isString() || message.toString().size() > 2048 || m_events.size() >= maxEvents)
            return fail(QStringLiteral("事件文件包含无效记录或超过 100000 条上限"));
        m_events.append({value, type, message.toString()});
    }
    if (file.error() != QFile::NoError)
        return fail(file.errorString());
    m_writtenCount = m_events.size();
    return true;
}

bool SessionEvents::append(double time, const QString &type, const QString &message)
{
    if (m_readOnly || m_path.isEmpty() || !qIsFinite(time) || time < 0 || type.isEmpty() ||
        type.size() > 32 || message.size() > 2048 || m_events.size() >= maxEvents)
    {
        m_error = QStringLiteral("事件参数无效、会话只读或超过 100000 条上限");
        return false;
    }
    m_events.append({time, type, message});
    return flush();
}

bool SessionEvents::flush()
{
    if (m_readOnly || m_path.isEmpty() || !hasPending())
        return true;
    if (!m_writtenBytes)
    {
        // 首次写入/另存用原子替换，避免遗留同名配套文件的旧事件。
        QSaveFile file(m_path);
        if (!file.open(QIODevice::WriteOnly))
        {
            m_error = file.errorString();
            return false;
        }
        bool ok = file.write(header()) == header().size();
        for (const auto &event : m_events)
        {
            const auto bytes = line(event);
            ok = ok && file.write(bytes) == bytes.size();
        }
        const auto size = file.pos();
        if (!ok || !file.commit())
        {
            m_error = QStringLiteral("保存事件失败：%1").arg(file.errorString());
            return false;
        }
        m_writtenBytes = size;
    }
    else
    {
        QFile file(m_path);
        if (!file.open(QIODevice::ReadWrite) || file.size() < m_writtenBytes ||
            !file.resize(m_writtenBytes) || !file.seek(m_writtenBytes))
        {
            m_error = QStringLiteral("无法追加事件（文件不可写、被删除或已缩短）：%1").arg(file.errorString());
            return false;
        }
        for (qsizetype i = m_writtenCount; i < m_events.size(); ++i)
        {
            const auto bytes = line(m_events[i]);
            if (file.write(bytes) != bytes.size())
            {
                m_error = QStringLiteral("追加事件失败：%1").arg(file.errorString());
                return false;
            }
        }
        if (!file.flush())
        {
            m_error = file.errorString();
            return false;
        }
        m_writtenBytes = file.pos();
    }
    m_writtenCount = m_events.size();
    m_error.clear();
    return true;
}

bool SessionEvents::relocate(const QString &session)
{
    m_path = fileName(session);
    m_writtenCount = m_writtenBytes = 0;
    const bool wasReadOnly = m_readOnly;
    m_readOnly = false;
    const bool ok = flush();
    // 只读会话另存后仍为只读；失败时保持可重试以免丢失待保存事件。
    m_readOnly = wasReadOnly && ok;
    return ok;
}
