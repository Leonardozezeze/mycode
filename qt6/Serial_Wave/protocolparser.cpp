#include "protocolparser.h"

#include <QtEndian>

#include <cstring>

namespace {

constexpr uchar kHeader     = 0xFF;
constexpr uchar kTerminator = 0xEE;
constexpr int   kFloatCount = 4;   // 破碎率 / 含杂率 / 实际转速 / 目标转速
constexpr int   kFloatBytes = 4;
constexpr int   kFrameBytes = 1 + kFloatCount * kFloatBytes + 1;   // 18

static_assert(sizeof(float) == 4, "协议假定 float 为 32 位");

// 小端序读取一个 IEEE-754 单精度浮点数
double readFloatLE(const char *bytes)
{
    const quint32 bits = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(bytes));
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return double(value);
}

} // namespace

ProtocolParser::ProtocolParser() = default;

void ProtocolParser::setSampleRate(double hz)
{
    if (hz > 0.0)
        m_sampleRateHz = hz;
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

    while (true) {
        // 找到帧头；帧头之前的数据都是无效数据，直接丢掉
        const int header = m_buffer.indexOf(char(kHeader));
        if (header < 0) {
            m_buffer.clear();
            break;
        }
        if (header > 0)
            m_buffer.remove(0, header);

        // 半帧：等下一批串口数据
        if (m_buffer.size() < kFrameBytes)
            break;

        // 帧尾不对：可能锁到了数据里的假 0xFF，丢掉 1 字节重新同步
        if (uchar(m_buffer.at(kFrameBytes - 1)) != kTerminator) {
            ++m_errors;
            m_buffer.remove(0, 1);
            continue;
        }

        double values[kFloatCount];
        bool valid = true;
        for (int i = 0; i < kFloatCount; ++i) {
            values[i] = readFloatLE(
                m_buffer.constData() + 1 + i * kFloatBytes);
            if (!qIsFinite(values[i])) {
                valid = false;
                break;
            }
        }

        if (!valid) {
            ++m_errors;
            m_buffer.remove(0, 1);
            continue;
        }

        handleFrame(values[0], values[1], values[2], values[3]);
        m_buffer.remove(0, kFrameBytes);
    }
}

void ProtocolParser::handleFrame(double crushRate, double impurityRate,
                                 double rpm, double targetRpm)
{
    ProtocolFrame frame;
    frame.index        = m_frameIndex++;
    frame.time         = double(frame.index) / m_sampleRateHz;
    frame.crushRate    = crushRate;
    frame.impurityRate = impurityRate;
    frame.rpm          = rpm;
    frame.targetRpm    = targetRpm;

    ++m_frames;
    m_crushSum    += crushRate;
    m_impuritySum += impurityRate;

    // 每满 1 秒统计一次实际更新频率
    ++m_rateFrames;
    if (!m_rateClock.isValid()) {
        m_rateClock.start();
    } else if (m_rateClock.elapsed() >= 1000) {
        const qint64 elapsedMs = m_rateClock.restart();
        if (elapsedMs > 0)
            m_updateHz = double(m_rateFrames) * 1000.0 / double(elapsedMs);
        m_rateFrames = 0;
    }

    if (m_frameHandler)
        m_frameHandler(frame);

    // 状态：协议里没有电机电流，保持 NaN，界面会显示 "--"
    MachineStatus status;
    status.crushRate        = crushRate;
    status.crushRateAvg     = m_crushSum / double(m_frames);
    status.impurityRate     = impurityRate;
    status.impurityRateAvg  = m_impuritySum / double(m_frames);
    status.rpm              = rpm;
    status.rpmTarget        = targetRpm;
    status.running          = qAbs(rpm) > 0.0;   // 正反转都算运行
    status.frames           = m_frames;
    status.errors           = m_errors;
    status.updateHz         = m_updateHz;

    if (m_statusHandler)
        m_statusHandler(status);
}
