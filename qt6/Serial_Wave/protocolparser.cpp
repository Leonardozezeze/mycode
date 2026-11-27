#include "protocolparser.h"

#include <QtEndian>

#include <cstring>

namespace
{

constexpr uchar kHeader = 0xFF;
constexpr uchar kActuatorHeader = 0xFE;
constexpr uchar kTerminator = 0xEE;
constexpr int kFloatCount = 4; // 破碎率 / 含杂率 / 实际转速 / 目标转速
constexpr int kFloatBytes = 4;
constexpr int kFrameBytes = 1 + kFloatCount * kFloatBytes + 1; // 18

static_assert(sizeof(float) == 4, "协议假定 float 为 32 位");

// 小端序读取一个 IEEE-754 单精度浮点数
double readFloatLE(const char *bytes)
{
    const quint32 bits = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes));
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return double(value);
}

} // namespace

ProtocolParser::ProtocolParser()
{
    resetSession();
}

void ProtocolParser::resetSession(const QElapsedTimer &clock)
{
    resetStream();
    m_frameIndex = m_frames = m_errors = 0;
    m_crushSum = m_impuritySum = 0.0;
    m_qualityFrames = 0;
    if (clock.isValid())
        m_sessionClock = clock;
    else
        m_sessionClock.start();
}

void ProtocolParser::setFrameHandler(FrameHandler handler)
{
    m_frameHandler = std::move(handler);
}

void ProtocolParser::setStatusHandler(StatusHandler handler)
{
    m_statusHandler = std::move(handler);
}

void ProtocolParser::resetStream()
{
    m_buffer.clear();
    m_rateClock.invalidate();
    m_rateFrames = 0;
    m_updateHz = qQNaN();
    m_latestCan = {};
    m_canUpdateTime.fill(qQNaN());
}

void ProtocolParser::feed(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    m_buffer.append(data);

    while (true)
    {
        // 找到帧头；帧头之前的数据都是无效数据，直接丢掉
        const int legacy = m_buffer.indexOf(char(kHeader));
        const int extended = m_buffer.indexOf(char(kActuatorHeader));
        const int header = legacy < 0 ? extended : extended < 0 ? legacy : qMin(legacy, extended);
        if (header < 0)
        {
            if (m_textHandler)
                m_textHandler(m_buffer);
            m_buffer.clear();
            break;
        }
        if (header > 0)
        {
            if (m_textHandler)
                m_textHandler(m_buffer.left(header));
            m_buffer.remove(0, header);
        }

        // FE + 原四路 float + 行程百分数 float + 状态 byte + EE（23 字节）。
        // 用独立帧头避免把旧 18 字节包与扩展包按长度猜测。
        const bool actuator = uchar(m_buffer[0]) == kActuatorHeader;
        const int frameBytes = actuator ? 23 : kFrameBytes;
        const int floatCount = actuator ? 5 : kFloatCount;
        // 半帧：等下一批串口数据
        if (m_buffer.size() < frameBytes)
            break;

        // 帧尾不对：可能锁到了数据里的假 0xFF，丢掉 1 字节重新同步
        if (uchar(m_buffer.at(frameBytes - 1)) != kTerminator)
        {
            ++m_errors;
            m_buffer.remove(0, 1);
            continue;
        }

        double values[5] = {};
        bool valid = true;
        for (int i = 0; i < floatCount; ++i)
        {
            values[i] = readFloatLE(m_buffer.constData() + 1 + i * kFloatBytes);
            if (!qIsFinite(values[i]))
            {
                valid = false;
                break;
            }
        }

        if (actuator && (values[4] < 0 || values[4] > 100 ||
                         uchar(m_buffer[21]) > uchar(ActuatorState::Fault)))
            valid = false;
        if (!valid)
        {
            ++m_errors;
            m_buffer.remove(0, 1);
            continue;
        }

        // 协议传输 0～1 的原始比例；在解析边界统一转成百分数。
        // 波形、状态统计和会话记录中的品质通道均使用 %。
        TelemetrySample frame;
        frame.crushRate = values[0] * 100.0;
        frame.impurityRate = values[1] * 100.0;
        frame.rpm = values[2];
        frame.targetRpm = values[3];
        if (actuator)
        {
            frame.actuatorPosition = values[4];
            frame.actuatorState = ActuatorState(uchar(m_buffer[21]));
        }
        handleFrame(frame);
        m_buffer.remove(0, frameBytes);
    }
}

void ProtocolParser::feedCan(const QByteArray &payload, CanTelemetry kind)
{
    if (payload.size() != (kind == CanTelemetry::Actuator ? kFloatBytes + 1 : 2 * kFloatBytes))
    {
        ++m_errors;
        return;
    }
    const double first = readFloatLE(payload.constData());
    const double second = kind == CanTelemetry::Actuator
        ? double(uchar(payload[kFloatBytes])) : readFloatLE(payload.constData() + kFloatBytes);
    if (!qIsFinite(first) || !qIsFinite(second) ||
        (kind == CanTelemetry::Quality && (first < 0 || first > 1 || second < 0 || second > 1)) ||
        (kind == CanTelemetry::Actuator && (first < 0 || first > 100 ||
                                          second > int(ActuatorState::Fault))))
    {
        ++m_errors;
        return;
    }

    TelemetrySample frame;
    switch (kind)
    {
    case CanTelemetry::Quality:
        frame.impurityRate = first * 100;
        frame.crushRate = second * 100;
        break;
    case CanTelemetry::Speed:
        frame.rpm = first;
        frame.targetRpm = second;
        break;
    case CanTelemetry::Actuator:
        frame.actuatorPosition = first;
        frame.actuatorState = ActuatorState(int(second));
        break;
    }
    handleFrame(frame, kind);
}

void ProtocolParser::handleFrame(TelemetrySample frame, std::optional<CanTelemetry> kind)
{
    frame.index = m_frameIndex++;
    frame.time = double(m_sessionClock.nsecsElapsed()) / 1e9;
    ++m_frames;
    if (qIsFinite(frame.crushRate))
    {
        ++m_qualityFrames;
        m_crushSum += frame.crushRate;
        m_impuritySum += frame.impurityRate;
    }

    // 每满 1 秒统计一次实际更新频率
    if (!m_rateClock.isValid())
    {
        m_rateClock.start();
        m_rateFrames = 0;
    }
    else
    {
        ++m_rateFrames;
    }
    if (m_rateClock.elapsed() >= 1000)
    {
        const qint64 elapsedMs = m_rateClock.restart();
        if (elapsedMs > 0)
            m_updateHz = double(m_rateFrames) * 1000.0 / double(elapsedMs);
        m_rateFrames = 0;
    }

    if (m_frameHandler)
        m_frameHandler(frame);

    // 每个 CAN 报文独立采样；状态卡显示各报文最近一次有效值。
    auto latest = frame;
    if (kind)
    {
        switch (*kind)
        {
        case CanTelemetry::Quality:
            m_latestCan.crushRate = frame.crushRate;
            m_latestCan.impurityRate = frame.impurityRate;
            break;
        case CanTelemetry::Speed:
            m_latestCan.rpm = frame.rpm;
            m_latestCan.targetRpm = frame.targetRpm;
            break;
        case CanTelemetry::Actuator:
            m_latestCan.actuatorPosition = frame.actuatorPosition;
            m_latestCan.actuatorState = frame.actuatorState;
            break;
        }
        m_canUpdateTime[int(*kind)] = frame.time;
        const auto fresh = [&](CanTelemetry channel)
        { return frame.time - m_canUpdateTime[int(channel)] < 2.0; };
        if (fresh(CanTelemetry::Quality))
        {
            latest.crushRate = m_latestCan.crushRate;
            latest.impurityRate = m_latestCan.impurityRate;
        }
        if (fresh(CanTelemetry::Speed))
        {
            latest.rpm = m_latestCan.rpm;
            latest.targetRpm = m_latestCan.targetRpm;
        }
        if (fresh(CanTelemetry::Actuator))
        {
            latest.actuatorPosition = m_latestCan.actuatorPosition;
            latest.actuatorState = m_latestCan.actuatorState;
        }
    }
    MachineStatus status;
    status.crushRate = latest.crushRate;
    status.impurityRate = latest.impurityRate;
    if (m_qualityFrames)
    {
        status.crushRateAvg = m_crushSum / double(m_qualityFrames);
        status.impurityRateAvg = m_impuritySum / double(m_qualityFrames);
    }
    status.rpm = latest.rpm;
    status.rpmTarget = latest.targetRpm;
    status.actuatorPosition = latest.actuatorPosition;
    status.actuatorState = latest.actuatorState;
    status.running = qAbs(latest.rpm) > 0.0; // 正反转都算运行
    status.frames = m_frames;
    status.errors = m_errors;
    status.updateHz = m_updateHz;

    if (m_statusHandler)
        m_statusHandler(status);
}
