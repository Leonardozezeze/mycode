#pragma once

#include <QWidget>
#include <QVector>
#include <QPointF>

// 参数趋势显示组件（损失率 + 风机转速）
class ParameterTrendWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ParameterTrendWidget(QWidget *parent = nullptr);
    ~ParameterTrendWidget() override;

    // 添加数据点
    void addDataPoint(quint64 timestamp, double lossRate, double fanRpm);

    // 清空数据
    void clear();

    // 设置显示时间窗口（毫秒）
    void setTimeWindow(quint64 ms);
    quint64 timeWindow() const { return m_timeWindow; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct DataPoint {
        quint64 timestamp;
        double lossRate;
        double fanRpm;
    };

    void drawGrid(QPainter &painter);
    void drawLossRateCurve(QPainter &painter);
    void drawFanRpmCurve(QPainter &painter);
    void updateViewRange();

    QVector<DataPoint> m_data;

    quint64 m_timeWindow = 30000; // 默认显示 30 秒
    bool m_autoScroll = true;

    // 视图范围
    double m_minLossRate = 1.5;
    double m_maxLossRate = 2.8;
    double m_minFanRpm = 0.0;
    double m_maxFanRpm = 350.0;
    quint64 m_startTime = 0;
    quint64 m_endTime = 30000;

    // 边距
    int m_marginLeft = 70;
    int m_marginRight = 70;
    int m_marginTop = 30;
    int m_marginBottom = 50;
};
