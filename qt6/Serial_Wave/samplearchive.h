#ifndef SAMPLEARCHIVE_H
#define SAMPLEARCHIVE_H

#include <QString>
#include <QVector>

#include <functional>

class QTemporaryFile;
class QDataStream;

// 多通道波形采样归档。
//
// 每个采样点的记录格式固定为：时间 x + channelCount 个通道值。
// 内存里只保留一小撮尚未落盘的采样；攒够一定字节数就追加写入临时
// 缓冲文件，长时间采集时内存占用有上限。导出 / 回看历史时按需要
// 从文件里流式读出。
//
// 缓冲文件是本次运行的临时文件（进程退出自动删除），clear() 会重建它。
class SampleArchive
{
public:
    explicit SampleArchive(int channelCount = 1);
    ~SampleArchive();

    SampleArchive(const SampleArchive &) = delete;
    SampleArchive &operator=(const SampleArchive &) = delete;

    int channelCount() const { return m_channelCount; }

    // 未落盘内存缓冲达到这个字节数就自动写一次缓冲文件
    static constexpr qint64 flushThresholdBytes = 64 * 1024;

    // 缓冲文件当前大小（字节）；写缓冲每次写完都会 flush，数值是准的
    qint64 bufferSize() const;

    // 追加一个采样点。values 必须有 channelCount() 个元素。
    void append(double x, const double *values);

    // 清空全部记录并重建缓冲文件
    void clear();

    // 当前总采样点数（已落盘 + 内存缓冲）
    qint64 count() const { return m_flushedCount + pendingCount(); }

    // 把内存缓冲追加写入缓冲文件。返回 false 时 errorString() 有原因，
    // 此时缓冲里的数据仍保留在内存中，不会丢。
    bool flush();

    // 按写入顺序遍历全部采样。调用前会先 flush 内存缓冲。
    // values 指向 channelCount() 个 double，仅在回调期间有效。
    // 回调返回 false 可提前停止遍历。返回 false 表示读文件失败。
    bool forEachSample(const std::function<bool(double x, const double *values)> &fn);

    // 只遍历 x ∈ [xMin, xMax] 的采样（按 x 递增的有序文件里二分定位）。
    // 用于暂停后拖拽 / 缩放查看历史数据、以及切换显示通道。调用前会先 flush。
    bool forEachSampleInRange(double xMin, double xMax,
                              const std::function<bool(double x, const double *values)> &fn);

    // 最近一个采样点的 x（时间）。没有数据时返回 false。
    bool lastX(double &x) const;

    QString errorString() const { return m_error; }
    QString bufferFileName() const;

private:
    int  recordDoubles() const { return 1 + m_channelCount; }
    qint64 recordBytes() const { return qint64(recordDoubles()) * qint64(sizeof(double)); }
    qint64 pendingCount() const { return m_pending.size() / recordDoubles(); }

    bool openBufferFile();
    bool writeSamples(const QVector<double> &flatSamples);
    bool readSampleAt(qint64 index, double &x);
    bool readRecord(QDataStream &in, double &x);

    QTemporaryFile *m_file         = nullptr;
    int              m_channelCount = 1;
    QVector<double>  m_pending;     // 尚未落盘的平铺数据：x, v0..vN, x, v0..vN, ...
    qint64           m_flushedCount = 0;
    QVector<double>  m_readValues;  // 读取时复用的通道值暂存
    double           m_lastX       = 0.0;
    QString          m_error;
};

#endif // SAMPLEARCHIVE_H
