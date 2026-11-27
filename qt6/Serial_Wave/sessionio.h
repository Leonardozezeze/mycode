#ifndef SESSIONIO_H
#define SESSIONIO_H
#include <QString>
#include <QVector>
#include <array>
#include <atomic>
#include <memory>
#include "telemetry.h"

struct SamplePoint
{
    double time = 0;
    std::array<double, sampleChannelCount> values = []
    {
        std::array<double, sampleChannelCount> v;
        v.fill(qQNaN());
        return v;
    }();
    std::array<bool, waveformChannelCount> breakBefore{};

    bool hasPlotPoint(int channel) const
    {
        // 完整串口帧未带推杆反馈时，应保留这一处明确的缺失值。
        // CAN 的单组报文只影响自己携带的通道。
        return qIsFinite(values[channel]) ||
               (channel == 4 && qIsFinite(values[0]) && qIsFinite(values[2]));
    }

    bool markChannelGaps(std::array<double, waveformChannelCount> &lastTimes, double gap)
    {
        bool hasGap = false;
        for (int c = 0; c < waveformChannelCount; ++c)
        {
            if (!qIsFinite(values[c]))
                continue;
            breakBefore[c] = lastTimes[c] >= 0 && time - lastTimes[c] > gap;
            lastTimes[c] = time;
            hasGap |= breakBefore[c];
        }
        return hasGap;
    }
};
struct ChannelStats
{
    double min = 0, max = 0, mean = 0, m2 = 0;
    qint64 count = 0;
};
struct ViewData
{
    QVector<SamplePoint> points;
    std::array<ChannelStats, sampleChannelCount> stats{};
    qint64 count = 0;
    QString error;
    double archiveLastTime = 0;
};
struct ExportState
{
    std::atomic_bool cancelled{false};
    std::atomic<qint64> done{0};
    qint64 total = 0;
    bool selectedRange = false;
    double lower = 0, upper = 0;
};
struct ExportResult
{
    bool ok = false;
    bool cancelled = false;
    QString error;
};

struct MeasurementData
{
    SamplePoint a, b;
    bool hasA = false, hasB = false;
    std::array<ChannelStats, sampleChannelCount> stats{};
    qint64 count = 0;
    QString error;
};

MeasurementData readSessionMeasurement(const QString &path, double a, bool hasB, double b,
                                       const std::shared_ptr<std::atomic_bool> &cancelled);

// 工作线程各自只读打开文件，不共享采集器的 QFile/缓冲或 GUI 对象。
ViewData readSessionView(const QString &path, double lower, double upper,
                         const std::shared_ptr<std::atomic_bool> &cancelled);
ExportResult exportSession(const QString &source, const QString &destination,
                           const std::shared_ptr<ExportState> &state);
#endif
