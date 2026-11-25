#include "sessioncleanup.h"
#include "samplearchive.h"
#include "sessionevents.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace
{
bool protectedPath(const QString &path, const QStringList &protectedSessions)
{
    const QFileInfo file(path);
    for (const auto &session : protectedSessions)
    {
        if (session.isEmpty()) continue;
        const QFileInfo active(session);
        if (file.absoluteFilePath() == active.absoluteFilePath() ||
            (!file.canonicalFilePath().isEmpty() && file.canonicalFilePath() == active.canonicalFilePath()))
            return true;
    }
    return false;
}

bool snapshot(const QFileInfo &info, const QString &session, bool main, CleanupFile &result)
{
    if (!info.isFile() || info.isSymLink()) return false;
    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) return false;
    result = {info.absoluteFilePath(), session, info.size(), info.lastModified(), file.read(128), main};
    return file.error() == QFile::NoError;
}
}

SessionCleanupPlan planSessionCleanup(const QString &directory, const QStringList &protectedSessions)
{
    SessionCleanupPlan plan;
    QDir dir(directory);
    plan.directory = dir.canonicalPath();
    if (dir.exists() && !dir.isReadable())
    {
        plan.error = QStringLiteral("记录目录不可读取");
        return plan;
    }
    if (plan.directory.isEmpty())
    {
        if (dir.exists()) plan.error = QStringLiteral("无法访问记录目录");
        return plan;
    }
    for (const auto &info : dir.entryInfoList({"*.swave"}, QDir::Files | QDir::NoSymLinks, QDir::Name))
    {
        const auto session = info.absoluteFilePath();
        if (protectedPath(session, protectedSessions))
        {
            ++plan.protectedSessions;
            continue;
        }
        SampleArchive archive;
        if (!archive.open(session)) continue; // 不删除未知格式或损坏文件
        CleanupFile main;
        if (!snapshot(info, session, true, main)) continue;
        plan.files.append(main);
        plan.bytes += main.bytes;
        ++plan.sessions;
        CleanupFile events;
        if (snapshot(QFileInfo(SessionEvents::fileName(session)), session, false, events))
        {
            plan.files.append(events);
            plan.bytes += events.bytes;
        }
    }
    return plan;
}

SessionCleanupResult executeSessionCleanup(const SessionCleanupPlan &plan,
                                           const QStringList &protectedSessions)
{
    SessionCleanupResult result;
    if (!plan.error.isEmpty() || plan.directory.isEmpty() ||
        QDir(plan.directory).canonicalPath() != plan.directory)
    {
        result.errors << QStringLiteral("记录目录已变化或不可访问，未删除文件");
        return result;
    }
    QStringList removedSessions;
    for (const auto &entry : plan.files)
    {
        const QFileInfo info(entry.path);
        CleanupFile current;
        if (protectedPath(entry.sessionPath, protectedSessions) ||
            QDir(info.absolutePath()).canonicalPath() != plan.directory ||
            (!entry.session && !removedSessions.contains(entry.sessionPath)))
        {
            ++result.skipped;
            continue;
        }
        if (!snapshot(info, entry.sessionPath, entry.session, current) ||
            current.bytes != entry.bytes || current.modified != entry.modified || current.header != entry.header)
        {
            ++result.skipped;
            result.errors << QStringLiteral("文件已变化或不可读取，跳过：%1").arg(info.fileName());
            continue;
        }
        QFile file(entry.path);
        if (!file.remove())
        {
            result.errors << QStringLiteral("删除失败 %1：%2").arg(info.fileName(), file.errorString());
            continue;
        }
        ++result.filesRemoved;
        if (entry.session)
        {
            ++result.sessionsRemoved;
            removedSessions << entry.sessionPath;
        }
    }
    return result;
}
