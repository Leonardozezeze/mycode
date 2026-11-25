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
    bool breakBefore = false;
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
