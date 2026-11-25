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

void ProtocolParser::feedCan(const QByteArray &payload)
{
    if (payload.size() != 2 * kFloatBytes)
    {
        ++m_errors;
        return;
    }
    const double impurity = readFloatLE(payload.constData());
    const double crush = readFloatLE(payload.constData() + kFloatBytes);
    if (!(impurity >= 0.0 && impurity <= 1.0 && crush >= 0.0 && crush <= 1.0))
    {
        ++m_errors;
        return;
    }

    TelemetrySample frame;
    frame.impurityRate = impurity * 100.0;
    frame.crushRate = crush * 100.0;
    handleFrame(frame);
}

void ProtocolParser::handleFrame(TelemetrySample frame)
{
    frame.index = m_frameIndex++;
    frame.time = double(m_sessionClock.nsecsElapsed()) / 1e9;
    const double crushRate = frame.crushRate, impurityRate = frame.impurityRate;
    const double rpm = frame.rpm, targetRpm = frame.targetRpm;

    ++m_frames;
    m_crushSum += crushRate;
    m_impuritySum += impurityRate;

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

    // 状态：协议里没有电机电流，保持 NaN，界面会显示 "--"
    MachineStatus status;
    status.crushRate = crushRate;
    status.crushRateAvg = m_crushSum / double(m_frames);
    status.impurityRate = impurityRate;
    status.impurityRateAvg = m_impuritySum / double(m_frames);
    status.rpm = rpm;
    status.rpmTarget = targetRpm;
    status.actuatorPosition = frame.actuatorPosition;
    status.actuatorState = frame.actuatorState;
    status.running = qAbs(rpm) > 0.0; // 正反转都算运行
    status.frames = m_frames;
    status.errors = m_errors;
    status.updateHz = m_updateHz;

    if (m_statusHandler)
        m_statusHandler(status);
}
