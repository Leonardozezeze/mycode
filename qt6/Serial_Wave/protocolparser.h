#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QtNumeric>

#include <functional>

#include "machinestatus.h"

// 一帧解析结果
struct ProtocolFrame
{
    quint64 index        = 0;     // 帧序号（从 0 开始，跨连接连续）
    double  time         = 0.0;   // 推算时间 = index / 采样率（秒）
    double  crushRate    = 0.0;   // 破碎率 %
    double  impurityRate = 0.0;   // 含杂率 %
    double  rpm          = 0.0;   // 电机实际转速 rpm
    double  targetRpm    = 0.0;   // 目标（设定）转速 rpm
};

// 二进制帧解析器（纯后端，不依赖任何界面类）。
//
// 帧格式（共 18 字节）：
//   0xFF | float 破碎率 | float 含杂率 | float 实际转速 | float 目标转速 | 0xEE
// 四个 float 都是 4 字节、小端序（STM32 默认）。
//
// 协议里没有时间戳和校验：
//   * 时间按“帧序号 / 采样率”推算，采样率默认 20 Hz，可用 setSampleRate() 改；
//   * 帧尾不匹配或 float 非法时丢弃并右移 1 字节重新找 0xFF 同步，
//     误判次数计入 errorCount()。
//
// feed() 里自己处理半帧、粘包、脏数据，调用方收到多少串口字节就喂多少。
class ProtocolParser
{
public:
    using FrameHandler  = std::function<void(const ProtocolFrame &)>;
    using StatusHandler = std::function<void(const MachineStatus &)>;

    ProtocolParser();

    // 标称采样率（Hz），用于把帧序号换算成时间。必须在开始解析前设置。
    void setSampleRate(double hz);

    void setFrameHandler(FrameHandler handler);
    void setStatusHandler(StatusHandler handler);

    // 串口收到多少喂多少；半帧会留在内部缓冲里等下一批字节
    void feed(const QByteArray &data);

    // 断开串口时调用：清掉残留半帧和速率统计窗口。
    // 不重置帧序号和平均值，保证时间轴在重连后仍然单调递增。
    void resetStream();

    quint64 frameCount() const { return m_frames; }
    quint64 errorCount() const { return m_errors; }

private:
    void handleFrame(double crushRate, double impurityRate,
                     double rpm, double targetRpm);

    double  m_sampleRateHz = 20.0;
    QByteArray m_buffer;

    quint64 m_frameIndex = 0;   // 下一帧的序号（跨连接连续）
    quint64 m_frames     = 0;   // 有效帧总数
    quint64 m_errors     = 0;   // 同步失败 / 非法帧次数

    double m_crushSum    = 0.0;
    double m_impuritySum = 0.0;

    QElapsedTimer m_rateClock;      // 测更新频率用
    quint64       m_rateFrames = 0;
    double        m_updateHz   = qQNaN();

    FrameHandler  m_frameHandler;
    StatusHandler m_statusHandler;
};

#endif // PROTOCOLPARSER_H
