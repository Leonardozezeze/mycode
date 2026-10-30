#pragma once

#include <QWidget>
#include <QVector>
#include <QPointF>
#include <QTimer>

// 波形显示组件
class WaveformWidget : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget *parent = nullptr);
    ~WaveformWidget() override;

    // 添加数据点
    void addDataPoint(quint64 timestamp, double value);

    // 添加滤波后的数据点（会自动插值，让曲线更平滑）
    void addFilteredPoint(quint64 timestamp, double value);

    // 清空数据
    void clear();

    // 设置显示时间窗口（毫秒）
    void setTimeWindow(quint64 ms);
    quint64 timeWindow() const { return m_timeWindow; }

    // 设置自动滚动
    void setAutoScroll(bool enable);
    bool autoScroll() const { return m_autoScroll; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct DataPoint {
        quint64 timestamp;
        double value;
    };

    void drawGrid(QPainter &painter);
    void drawWaveform(QPainter &painter, const QVector<DataPoint> &data,
                     const QColor &color, const QString &label, int lineWidth = 2, bool smooth = false);
    void updateViewRange();

    QVector<DataPoint> m_rawData;
    QVector<DataPoint> m_filteredData;

    quint64 m_timeWindow = 5000; // 默认显示 5 秒
    bool m_autoScroll = true;

    // 视图范围
    double m_minValue = 0.0;
    double m_maxValue = 100.0;
    quint64 m_startTime = 0;
    quint64 m_endTime = 10000;

    // 边距
    int m_marginLeft = 60;
    int m_marginRight = 20;
    int m_marginTop = 20;
    int m_marginBottom = 40;
};
