#include "samplearchive.h"

#include <QDataStream>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryFile>

namespace {

constexpr qint64 kDoubleBytes = qint64(sizeof(double));

} // namespace

SampleArchive::SampleArchive(int channelCount)
    : m_channelCount(qMax(1, channelCount))
    , m_readValues(m_channelCount, 0.0)
{
    openBufferFile();
}

SampleArchive::~SampleArchive()
{
    delete m_file;   // QTemporaryFile 析构时自动删除文件
}

bool SampleArchive::openBufferFile()
{
    delete m_file;
    m_file = nullptr;

    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    m_file = new QTemporaryFile(
        QDir(dir).filePath(QStringLiteral("Serial_Wave_XXXXXX.dat")));
    m_file->setAutoRemove(true);

    if (!m_file->open()) {
        m_error = m_file->errorString();
        return false;
    }
    m_error.clear();
    m_file->setTextModeEnabled(false);
    return true;
}

void SampleArchive::append(double x, const double *values)
{
    m_lastX = x;
    m_pending.append(x);
    for (int c = 0; c < m_channelCount; ++c)
        m_pending.append(values[c]);

    // 内存缓冲到上限就落盘，避免长时间采集内存一直涨
    if (m_pending.size() * kDoubleBytes >= flushThresholdBytes)
        flush();
}

void SampleArchive::clear()
{
    m_pending.clear();
    m_flushedCount = 0;
    m_lastX = 0.0;
    m_error.clear();
    openBufferFile();   // 旧的临时文件随 delete 一起删除，新开一个空文件
}

bool SampleArchive::writeSamples(const QVector<double> &flatSamples)
{
    if (flatSamples.isEmpty())
        return true;

    if (!m_file || !m_file->isOpen()) {
        if (!openBufferFile())
            return false;
    }

    // 导出读取过文件后位置不在末尾。按“已落盘record数 × 每条记录字节数”
    // 定位，不用 size()：QFile 有写缓冲，size() 可能还没把缓冲算进去。
    if (!m_file->seek(m_flushedCount * recordBytes())) {
        m_error = m_file->errorString();
        return false;
    }

    bool writeOk = true;
    {
        QDataStream out(m_file);
        out.setByteOrder(QDataStream::LittleEndian);
        out.setFloatingPointPrecision(QDataStream::DoublePrecision);
        for (double v : flatSamples)
            out << v;
        writeOk = out.status() == QDataStream::Ok;
    }

    if (!writeOk) {
        m_error = QStringLiteral("写入采样缓冲失败");
        return false;
    }

    // QFile 写入有缓冲，显式 flush 后才算真正落盘
    if (!m_file->flush()) {
        m_error = m_file->errorString();
        return false;
    }
    return true;
}

bool SampleArchive::flush()
{
    m_error.clear();

    const qint64 pending = pendingCount();
    if (pending == 0)
        return true;

    if (!writeSamples(m_pending))
        return false;

    m_flushedCount += pending;
    m_pending.clear();
    return true;
}

bool SampleArchive::readRecord(QDataStream &in, double &x)
{
    in >> x;
    for (int c = 0; c < m_channelCount; ++c)
        in >> m_readValues[c];

    if (in.status() != QDataStream::Ok) {
        m_error = QStringLiteral("读取采样缓冲失败");
        return false;
    }
    return true;
}

bool SampleArchive::readSampleAt(qint64 index, double &x)
{
    if (!m_file || !m_file->isOpen()) {
        m_error = QStringLiteral("采样缓冲文件不可用");
        return false;
    }
    if (!m_file->seek(index * recordBytes())) {
        m_error = m_file->errorString();
        return false;
    }

    QDataStream in(m_file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::DoublePrecision);
    return readRecord(in, x);
}

bool SampleArchive::forEachSample(
    const std::function<bool(double x, const double *values)> &fn)
{
    if (!flush())
        return false;

    if (m_flushedCount == 0)
        return true;

    if (!m_file || !m_file->isOpen()) {
        m_error = QStringLiteral("采样缓冲文件不可用");
        return false;
    }
    if (!m_file->seek(0)) {
        m_error = m_file->errorString();
        return false;
    }

    QDataStream in(m_file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::DoublePrecision);

    for (qint64 i = 0; i < m_flushedCount; ++i) {
        double x = 0.0;
        if (!readRecord(in, x))
            return false;
        if (!fn(x, m_readValues.constData()))
            break;
    }
    return true;
}

bool SampleArchive::forEachSampleInRange(
    double xMin, double xMax,
    const std::function<bool(double x, const double *values)> &fn)
{
    if (!flush())
        return false;

    if (m_flushedCount == 0)
        return true;

    if (!m_file || !m_file->isOpen()) {
        m_error = QStringLiteral("采样缓冲文件不可用");
        return false;
    }

    // 记录是定长的并且按 x 递增写入，二分找第一个 x >= xMin 的记录
    qint64 lo = 0;
    qint64 hi = m_flushedCount;
    while (lo < hi) {
        const qint64 mid = lo + (hi - lo) / 2;
        double mx = 0.0;
        if (!readSampleAt(mid, mx))
            return false;
        if (mx < xMin)
            lo = mid + 1;
        else
            hi = mid;
    }

    if (lo >= m_flushedCount)
        return true;

    if (!m_file->seek(lo * recordBytes())) {
        m_error = m_file->errorString();
        return false;
    }

    QDataStream in(m_file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setFloatingPointPrecision(QDataStream::DoublePrecision);

    for (qint64 i = lo; i < m_flushedCount; ++i) {
        double x = 0.0;
        if (!readRecord(in, x))
            return false;
        if (x > xMax)
            break;
        if (!fn(x, m_readValues.constData()))
            break;
    }
    return true;
}

bool SampleArchive::lastX(double &x) const
{
    if (count() == 0)
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
