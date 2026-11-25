#ifndef TELEMETRY_H
#define TELEMETRY_H
#include <QtNumeric>
#include <array>

// 与串口/CAN 无关的采集模型。前四路保持原顺序，新增行程及设备上报的状态。
constexpr int sampleChannelCount = 6;
constexpr int waveformChannelCount = 5;
constexpr int waveformPlotCount = 3;
constexpr std::array<int, waveformPlotCount> plotGraphCounts{2, 2, 1};

enum class ActuatorState : int
{
    Unknown = 0,
    Stopped = 1,
    Extending = 2,
    Retracting = 3,
    FullyRetracted = 4,
    FullyExtended = 5,
    Fault = 6
};

struct TelemetrySample
{
    quint64 index = 0;
    double time = 0;
    double crushRate = qQNaN();
    double impurityRate = qQNaN();
    double rpm = qQNaN();
    double targetRpm = qQNaN();
    double actuatorPosition = qQNaN(); // 0～100%，NaN 表示此帧没有推杆反馈
    ActuatorState actuatorState = ActuatorState::Unknown;
    std::array<double, sampleChannelCount> values() const
    {
        return {crushRate, impurityRate, rpm, targetRpm, actuatorPosition,
                qIsFinite(actuatorPosition) ? double(actuatorState) : qQNaN()};
    }
};
#endif
