#ifndef SAMPLEARCHIVE_H
#define SAMPLEARCHIVE_H

#include <QDateTime>
#include <QString>
#include <QVector>
#include <functional>

class QFile;
class QDataStream;

// 持久会话格式：24 字节头（标识、版本、通道数、开始 UTC 毫秒），
// 后接定长小端 double 记录：主机接收时间（秒）+ N 路值。
// v1 为原四路，v2 为四路 + 推杆行程百分数 + 推杆状态代码（共六路）。
// 未写完的最后一条记录在重新打开时忽略，已完整写入的记录可以恢复。
class SampleArchive
{
public:
    explicit SampleArchive(int channelCount = 4);
    ~SampleArchive();
    SampleArchive(const SampleArchive &) = delete;
    SampleArchive &operator=(const SampleArchive &) = delete;

    static constexpr qint64 headerBytes = 24;
    static constexpr qint64 flushThresholdBytes = 64 * 1024;
    bool create(const QString &path, int channelCount = 0);
    bool open(const QString &path);
    // 将已保存记录与尚在内存的记录一并另存，并切换到新文件。
    // 不要求先向原磁盘 flush，可用于磁盘满后的恢复。
    bool saveAs(const QString &path);
    bool append(double x, const double *values);
    bool flush();
    bool forEachSample(const std::function<bool(double, const double *)> &fn);
    bool forEachSampleInRange(double xMin, double xMax,
                              const std::function<bool(double, const double *)> &fn);
    bool lastX(double &x) const;
    // 基于原始记录二分定位，不使用绘图抽稀点。闭区间计数，失败返回 -1。
    bool nearestSample(double time, double &sampleTime, QVector<double> &values);
    qint64 countInRange(double lower, double upper);
    qint64 count() const { return m_flushedCount + pendingCount(); }
    int channelCount() const { return m_channelCount; }
    QString bufferFileName() const;
    qint64 bufferSize() const;
    QString errorString() const { return m_error; }
    QDateTime startedAt() const { return m_startedAt; }
    bool isReadOnly() const { return m_readOnly; }
    qint64 ignoredTailBytes() const { return m_ignoredTailBytes; }

private:
    qint64 recordBytes() const { return (1 + m_channelCount) * qint64(sizeof(double)); }
    qint64 pendingCount() const { return m_pending.size() / (1 + m_channelCount); }
    bool readRecord(QDataStream &in, double &x);
    bool readSampleAt(qint64 index, double &x);
    qint64 boundIndex(double time, bool afterEqual);
    void close();

    QFile *m_file = nullptr;
    int m_channelCount;
    QVector<double> m_pending;
    QVector<double> m_readValues;
    qint64 m_flushedCount = 0;
    qint64 m_ignoredTailBytes = 0;
    double m_lastX = 0;
    bool m_readOnly = false;
    QDateTime m_startedAt;
    QString m_error;
};
#endif
