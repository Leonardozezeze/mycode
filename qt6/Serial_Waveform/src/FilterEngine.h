#pragma once

#include <deque>

// 滤波引擎：提供多种平滑算法
class FilterEngine
{
public:
    enum class FilterType {
        MovingAverage,   // 简单移动平均
        EMA,             // 指数移动平均
        None             // 不过滤
    };

    FilterEngine();
    ~FilterEngine() = default;

    void setFilterType(FilterType type);
    FilterType filterType() const { return m_type; }

    // 设置窗口大小（对 MovingAverage 有效）
    void setWindowSize(int size);
    int windowSize() const { return m_windowSize; }

    // 设置 EMA 的 alpha（0~1，越大越敏感）
    void setEmaAlpha(double alpha);
    double emaAlpha() const { return m_emaAlpha; }

    // 输入一个采样点，得到滤波后的值
    double process(double input);

    // 重置滤波器状态
    void reset();

private:
    FilterType m_type = FilterType::MovingAverage;
    int m_windowSize = 10;
    double m_emaAlpha = 0.2;

    std::deque<double> m_buffer;   // 移动平均窗口
    double m_emaPrev = 0.0;        // EMA 上一次输出
    bool m_emaInitialized = false;
};
