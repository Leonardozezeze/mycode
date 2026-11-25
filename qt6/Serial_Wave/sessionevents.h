#ifndef SESSIONEVENTS_H
#define SESSIONEVENTS_H
#include <QDateTime>
#include <QVector>
#include <QString>

struct SessionEvent
{
    double time = 0;
    QString type;
    QString message;
};

// 可选的 JSON Lines 配套文件，不修改原始波形文件格式。
// 写入失败时保留尚未写入的事件，重试先截去本次失败的部分写入。
class SessionEvents
{
public:
    static QString fileName(const QString &session) { return session + ".events.jsonl"; }
    void begin(const QString &session, const QDateTime &startedAt);
    bool load(const QString &session, const QDateTime &startedAt);
    bool append(double time, const QString &type, const QString &message);
    bool flush();
    bool relocate(const QString &session); // 另存后向新配套文件复制全部事件
    const QVector<SessionEvent> &entries() const { return m_events; }
    QString errorString() const { return m_error; }
    bool ignoredTail() const { return m_ignoredTail; }
    bool hasPending() const { return !m_readOnly && (m_writtenCount < m_events.size() || !m_writtenBytes); }

private:
    QByteArray header() const;
    static QByteArray line(const SessionEvent &event);
    QString m_path;
    QDateTime m_startedAt;
    QVector<SessionEvent> m_events;
    qsizetype m_writtenCount = 0;
    qint64 m_writtenBytes = 0;
    bool m_readOnly = false;
    bool m_ignoredTail = false;
    QString m_error;
};
#endif
