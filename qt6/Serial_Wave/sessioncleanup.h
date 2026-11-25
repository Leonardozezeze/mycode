#ifndef SESSIONCLEANUP_H
#define SESSIONCLEANUP_H
#include <QDateTime>
#include <QStringList>
#include <QVector>

struct CleanupFile
{
    QString path;
    QString sessionPath;
    qint64 bytes = 0;
    QDateTime modified;
    QByteArray header;
    bool session = false;
};
struct SessionCleanupPlan
{
    QString directory;
    QVector<CleanupFile> files;
    int sessions = 0;
    int protectedSessions = 0;
    qint64 bytes = 0;
    QString error;
};
struct SessionCleanupResult
{
    int sessionsRemoved = 0;
    int filesRemoved = 0;
    int skipped = 0;
    QStringList errors;
};

// 非递归扫描配置目录；只清理有效 .swave 和对应事件文件，保留其他导出/用户文件。
SessionCleanupPlan planSessionCleanup(const QString &directory, const QStringList &protectedSessions);
// 只执行确认时快照中的文件，再核对位置、大小、时间、头部及当前保护列表。
SessionCleanupResult executeSessionCleanup(const SessionCleanupPlan &plan,
                                           const QStringList &protectedSessions);
#endif
