#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QByteArray>
#include <QElapsedTimer>
#include <QtNumeric>

#include <functional>
#include <utility>

#include "machinestatus.h"

// 一帧解析结果
using ProtocolFrame = TelemetrySample;

// 二进制帧解析器（纯后端，不依赖任何界面类）。
//
// 帧格式（共 18 字节）：
//   0xFF | float 破碎率 | float 含杂率 | float 实际转速 | float 目标转速 | 0xEE
// 四个 float 都是 4 字节、小端序（STM32 默认）。
// 破碎率、含杂率传输 0～1 的原始比例，例如 0.021 表示 2.1%；转速单位为 rpm。
// 解析时将前两项乘以 100，回调、界面、统计和会话记录统一使用百分数。
// 可选推杆扩展帧（23 字节）：FE | 原四路 float | 行程 float（0～100）| 状态 u8 | EE。
// 独立帧头自动识别，旧帧的推杆值为 NaN、状态为 Unknown；扩展帧也没有 CRC。
//
// 协议里没有时间戳和校验：
//   * 时间使用主机单调时钟，包含断开和停止上报的间隔；不是设备采样时间；
//   * 帧尾不匹配或 float 非法时丢弃并右移 1 字节重新找 0xFF 同步，
//     误判次数计入 errorCount()。
//
// feed() 里自己处理半帧、粘包、脏数据，调用方收到多少串口字节就喂多少。
class ProtocolParser
{
public:
    using FrameHandler = std::function<void(const ProtocolFrame &)>;
    using StatusHandler = std::function<void(const MachineStatus &)>;
    using TextHandler = std::function<void(const QByteArray &)>;

    ProtocolParser();

    void setFrameHandler(FrameHandler handler);
    void setStatusHandler(StatusHandler handler);
    void setTextHandler(TextHandler handler) { m_textHandler = std::move(handler); }

    // 串口收到多少喂多少；半帧会留在内部缓冲里等下一批字节
    void feed(const QByteArray &data);
    // 每次传入一个已按 CAN ID 过滤的数据帧：小端 float32 含杂率、破碎率。
    void feedCan(const QByteArray &payload);

    // 断开串口时调用：清掉残留半帧和速率统计窗口。
    // 不重置本次会话时间和累计值。
    void resetStream();
    // 新建采集会话：同时重置时间、计数和均值。
    void resetSession(const QElapsedTimer &clock = {});

    quint64 frameCount() const { return m_frames; }
    quint64 errorCount() const { return m_errors; }

private:
    void handleFrame(TelemetrySample frame);

    QElapsedTimer m_sessionClock;
    QByteArray m_buffer;

    quint64 m_frameIndex = 0; // 下一帧的序号（跨连接连续）
    quint64 m_frames = 0;     // 有效帧总数
    quint64 m_errors = 0;     // 同步失败 / 非法帧次数

    double m_crushSum = 0.0;
    double m_impuritySum = 0.0;

    QElapsedTimer m_rateClock; // 测更新频率用
    quint64 m_rateFrames = 0;
    double m_updateHz = qQNaN();

    FrameHandler m_frameHandler;
    StatusHandler m_statusHandler;
    TextHandler m_textHandler;
};

#endif // PROTOCOLPARSER_H
