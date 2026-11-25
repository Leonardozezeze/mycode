#include "samplearchive.h"
#include "telemetry.h"
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QtNumeric>
#include <limits>

namespace
{
constexpr char magic[] = "SWAVE001";
void configure(QDataStream &stream)
{
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::DoublePrecision);
    stream.setVersion(QDataStream::Qt_6_5);
}
} // namespace

SampleArchive::SampleArchive(int channels)
    : m_channelCount(qMax(1, channels)), m_readValues(m_channelCount, 0.0)
{
}
SampleArchive::~SampleArchive()
{
    if (!m_readOnly)
        flush();
    close();
}
void SampleArchive::close()
{
    delete m_file;
    m_file = nullptr;
    m_pending.clear();
    m_flushedCount = 0;
    m_lastX = 0;
    m_ignoredTailBytes = 0;
}

bool SampleArchive::create(const QString &path, int channelCount)
{
    const int channels = channelCount > 0 ? channelCount : m_channelCount;
    if (channels != 4 && channels != sampleChannelCount)
    {
        m_error = QStringLiteral("不支持的会话通道数");
        return false;
    }
    if (!m_readOnly && !flush())
        return false;
    auto *file = new QFile(path);
    if (!file->open(QIODevice::ReadWrite | QIODevice::NewOnly))
    {
        m_error = file->errorString();
        delete file;
        return false;
    }
    QDataStream out(file);
    configure(out);
    const auto started = QDateTime::currentDateTimeUtc();
    out.writeRawData(magic, 8);
    out << quint32(channels == 4 ? 1 : 2) << quint32(channels) << started.toMSecsSinceEpoch();
    if (out.status() != QDataStream::Ok || !file->flush())
    {
        m_error = file->errorString();
        file->remove();
        delete file;
        return false;
    }
    close();
    m_file = file;
    m_channelCount = channels;
    m_readValues.resize(channels);
    m_readOnly = false;
    m_startedAt = started;
    m_error.clear();
    return true;
}

bool SampleArchive::open(const QString &path)
{
    if (!m_readOnly && !flush())
        return false;
    // 先验证新文件，失败时保留当前会话。
    QFile probe(path);
    if (!probe.open(QIODevice::ReadOnly))
    {
        m_error = probe.errorString();
        return false;
    }
    QDataStream in(&probe);
    configure(in);
    char id[8] = {};
    quint32 version = 0, channels = 0;
    qint64 timestamp = 0;
    in.readRawData(id, 8);
    in >> version >> channels >> timestamp;
    if (in.status() != QDataStream::Ok || QByteArray(id, 8) != QByteArray(magic, 8) ||
        !((version == 1 && channels == 4) || (version == 2 && channels == sampleChannelCount)) ||
        probe.size() < headerBytes ||
        !QDateTime::fromMSecsSinceEpoch(timestamp).isValid())
    {
        m_error = QStringLiteral("会话格式或版本不受支持");
        return false;
    }
    const qint64 bytesPerRecord = (1 + channels) * qint64(sizeof(double));
    const qint64 records = (probe.size() - headerBytes) / bytesPerRecord;
    double last = 0;
    if (records > 0)
    {
        if (!probe.seek(headerBytes + (records - 1) * bytesPerRecord))
        {
            m_error = probe.errorString();
            return false;
        }
        in >> last;
        if (in.status() != QDataStream::Ok || !qIsFinite(last) || last < 0)
        {
            m_error = QStringLiteral("会话时间数据损坏");
            return false;
        }
    }
    auto *file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly))
    {
        m_error = file->errorString();
        delete file;
        return false;
    }
    close();
    m_file = file;
    m_readOnly = true;
    m_channelCount = int(channels);
    m_readValues.resize(channels);
    m_flushedCount = records;
    m_lastX = last;
    m_startedAt = QDateTime::fromMSecsSinceEpoch(timestamp).toUTC();
    m_ignoredTailBytes = (probe.size() - headerBytes) % recordBytes();
    m_error.clear();
    return true;
}

bool SampleArchive::append(double x, const double *values)
{
    if (!m_file || m_readOnly)
    {
        m_error = QStringLiteral("没有可写的采集会话");
        return false;
    }
    if (!qIsFinite(x) || x < 0 || (count() && x < m_lastX))
    {
        m_error = QStringLiteral("采样时间必须有限且单调递增");
        return false;
    }
    m_pending.append(x);
    for (int c = 0; c < m_channelCount; ++c)
        m_pending.append(values[c]);
    m_lastX = x;
    return m_pending.size() * qint64(sizeof(double)) < flushThresholdBytes || flush();
}

bool SampleArchive::saveAs(const QString &path)
{
    const QFileInfo source(bufferFileName()), destination(path);
    if (!m_file || source.absoluteFilePath() == destination.absoluteFilePath() ||
        (!destination.canonicalFilePath().isEmpty() &&
         source.canonicalFilePath() == destination.canonicalFilePath()))
    {
        m_error = QStringLiteral("请选择不同的会话保存路径");
        return false;
    }
    QFile input(bufferFileName());
    QSaveFile output(path);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
    {
        m_error = input.isOpen() ? output.errorString() : input.errorString();
        return false;
    }
    qint64 remaining = headerBytes + m_flushedCount * recordBytes();
    while (remaining > 0)
    {
        const auto block = input.read(qMin(remaining, qint64(64 * 1024)));
        if (block.isEmpty() || output.write(block) != block.size())
        {
            m_error = QStringLiteral("复制已保存的会话数据失败");
            return false;
        }
        remaining -= block.size();
    }
    QDataStream out(&output);
    configure(out);
    for (double value : m_pending)
        out << value;
    if (out.status() != QDataStream::Ok || !output.commit())
    {
        m_error = output.errorString();
        return false;
    }
    auto *replacement = new QFile(path);
    if (!replacement->open(m_readOnly ? QIODevice::ReadOnly : QIODevice::ReadWrite))
    {
        m_error = QStringLiteral("会话已另存，但无法重新打开：%1").arg(replacement->errorString());
        delete replacement;
        return false;
    }
    const qint64 savedCount = count();
    const double last = m_lastX;
    close();
    m_file = replacement;
    m_flushedCount = savedCount;
    m_lastX = last;
    m_error.clear();
    return true;
}

bool SampleArchive::flush()
{
    if (m_pending.isEmpty())
        return true;
    if (!m_file || m_readOnly)
    {
        m_error = QStringLiteral("采集会话不可写");
        return false;
    }
    if (!m_file->seek(headerBytes + m_flushedCount * recordBytes()))
    {
        m_error = m_file->errorString();
        return false;
    }
    QDataStream out(m_file);
    configure(out);
    for (double v : m_pending)
        out << v;
    if (out.status() != QDataStream::Ok || !m_file->flush())
    {
        m_error = QStringLiteral("保存会话失败：%1").arg(m_file->errorString());
        return false;
    }
    m_flushedCount += pendingCount();
    m_pending.clear();
    m_error.clear();
    return true;
}

bool SampleArchive::readRecord(QDataStream &in, double &x)
{
    in >> x;
    for (int c = 0; c < m_channelCount; ++c)
        in >> m_readValues[c];
    if (in.status() != QDataStream::Ok || !qIsFinite(x) || x < 0)
    {
        m_error = QStringLiteral("读取会话数据失败");
        return false;
    }
    return true;
}

bool SampleArchive::readSampleAt(qint64 index, double &x)
{
    if (!m_file || !m_file->seek(headerBytes + index * recordBytes()))
    {
        m_error = QStringLiteral("无法定位会话记录");
        return false;
    }
    QDataStream in(m_file);
    configure(in);
    return readRecord(in, x);
}

bool SampleArchive::forEachSample(const std::function<bool(double, const double *)> &fn)
{
    return forEachSampleInRange(0, std::numeric_limits<double>::max(), fn);
}

bool SampleArchive::forEachSampleInRange(double xMin, double xMax,
                                         const std::function<bool(double, const double *)> &fn)
{
    if (!flush())
        return false;
    if (!m_flushedCount)
        return true;
    qint64 lo = 0, hi = m_flushedCount;
    while (lo < hi)
    {
        const qint64 mid = lo + (hi - lo) / 2;
        double x;
        if (!readSampleAt(mid, x))
            return false;
        if (x < xMin)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo >= m_flushedCount)
        return true;
    if (!m_file->seek(headerBytes + lo * recordBytes()))
    {
        m_error = m_file->errorString();
        return false;
    }
    QDataStream in(m_file);
    configure(in);
    for (qint64 i = lo; i < m_flushedCount; ++i)
    {
        double x;
        if (!readRecord(in, x))
            return false;
        if (x > xMax)
            break;
        if (!fn(x, m_readValues.constData()))
            break;
    }
    return true;
}

qint64 SampleArchive::boundIndex(double time, bool afterEqual)
{
    qint64 lo = 0, hi = m_flushedCount;
    while (lo < hi)
    {
        const qint64 mid = lo + (hi - lo) / 2;
        double x;
        if (!readSampleAt(mid, x)) return -1;
        if (x < time || (afterEqual && x == time)) lo = mid + 1;
        else hi = mid;
    }
    return lo;
}

qint64 SampleArchive::countInRange(double lower, double upper)
{
    if (!qIsFinite(lower) || !qIsFinite(upper) || lower < 0 || upper < lower)
    {
        m_error = QStringLiteral("时间区间必须有限、非负且起点不晚于终点");
        return -1;
    }
    if (!flush()) return -1;
    const auto first = boundIndex(lower, false);
    if (first < 0) return -1;
    const auto end = boundIndex(upper, true);
    return end < 0 ? -1 : end - first;
}

bool SampleArchive::nearestSample(double time, double &sampleTime, QVector<double> &values)
{
    if (!qIsFinite(time) || time < 0)
    {
        m_error = QStringLiteral("定位时间必须有限且非负");
        return false;
    }
    if (!flush()) return false;
    if (!m_flushedCount)
    {
        m_error = QStringLiteral("会话没有采样数据");
        return false;
    }
    qint64 index = boundIndex(time, false);
    if (index < 0) return false;
    if (index == m_flushedCount) --index;
    else if (index > 0)
    {
        double before, after;
        if (!readSampleAt(index - 1, before) || !readSampleAt(index, after)) return false;
        if (time - before <= after - time) --index;
    }
    if (!readSampleAt(index, sampleTime)) return false;
    values = m_readValues;
    return true;
}

bool SampleArchive::lastX(double &x) const
{
    if (!count())
        return false;
    x = m_lastX;
    return true;
}
QString SampleArchive::bufferFileName() const
{
    return m_file ? m_file->fileName() : QString();
}
qint64 SampleArchive::bufferSize() const
{
    return m_file ? m_file->size() : 0;
}
