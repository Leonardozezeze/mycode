#include "WaveformWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QDebug>
#include <algorithm>
#include <limits>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(400, 300);
    setAutoFillBackground(true);
    setStyleSheet("background-color: #1e1e1e;");
}

WaveformWidget::~WaveformWidget() = default;

void WaveformWidget::addDataPoint(quint64 timestamp, double value)
{
    m_rawData.append({timestamp, value});

    // 限制数据点数量，防止内存溢出
    const int maxPoints = 100000;
    if (m_rawData.size() > maxPoints) {
        m_rawData.remove(0, m_rawData.size() - maxPoints);
    }

    updateViewRange();
    update();
}

void WaveformWidget::addFilteredPoint(quint64 timestamp, double value)
{
    // 对滤波数据进行插值，让曲线更平滑
    if (!m_filteredData.isEmpty()) {
        const auto &lastPoint = m_filteredData.last();
        quint64 timeDiff = timestamp - lastPoint.timestamp;

        // 如果时间间隔大于 5ms，插入中间点
        if (timeDiff > 5) {
            int numInterp = static_cast<int>(timeDiff / 5);  // 每 5ms 一个点
            numInterp = std::min(numInterp, 10);  // 最多插 10 个点

            for (int i = 1; i < numInterp; ++i) {
                double ratio = static_cast<double>(i) / numInterp;
                quint64 interpTime = lastPoint.timestamp + static_cast<quint64>(timeDiff * ratio);
                double interpValue = lastPoint.value + (value - lastPoint.value) * ratio;
                m_filteredData.append({interpTime, interpValue});
            }
        }
    }

    m_filteredData.append({timestamp, value});

    const int maxPoints = 200000;  // 增大限制
    if (m_filteredData.size() > maxPoints) {
        m_filteredData.remove(0, m_filteredData.size() - maxPoints);
    }

    update();
}

void WaveformWidget::clear()
{
    m_rawData.clear();
    m_filteredData.clear();
    m_minValue = 0.0;
    m_maxValue = 100.0;
    m_startTime = 0;
    m_endTime = m_timeWindow;
    update();
}

void WaveformWidget::setTimeWindow(quint64 ms)
{
    m_timeWindow = ms;
    updateViewRange();
    update();
}

void WaveformWidget::setAutoScroll(bool enable)
{
    m_autoScroll = enable;
    updateViewRange();
    update();
}

void WaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawGrid(painter);

    // 绘制原始波形（绿色，较细）
    drawWaveform(painter, m_rawData, QColor(76, 175, 80, 180), tr("原始"), 1, false);

    // 绘制滤波后波形（橙色，较粗，平滑）
    drawWaveform(painter, m_filteredData, QColor(255, 152, 0, 255), tr("滤波"), 3, true);
}

void WaveformWidget::drawGrid(QPainter &painter)
{
    QRect plotRect(m_marginLeft, m_marginTop,
                   width() - m_marginLeft - m_marginRight,
                   height() - m_marginTop - m_marginBottom);

    // 背景
    painter.fillRect(plotRect, QColor(30, 30, 30));

    // 网格线
    QPen gridPen(QColor(60, 60, 60));
    gridPen.setStyle(Qt::DotLine);
    painter.setPen(gridPen);

    // 垂直网格线（时间）
    const int numVerticalLines = 10;
    for (int i = 0; i <= numVerticalLines; ++i) {
        int x = plotRect.left() + (plotRect.width() * i) / numVerticalLines;
        painter.drawLine(x, plotRect.top(), x, plotRect.bottom());

        // 时间标签
        quint64 t = m_startTime + (m_endTime - m_startTime) * i / numVerticalLines;
        painter.setPen(QColor(150, 150, 150));
        painter.drawText(x - 20, plotRect.bottom() + 15, QString::number(t));
        painter.setPen(gridPen);
    }

    // 水平网格线（数值）
    const int numHorizontalLines = 8;
    for (int i = 0; i <= numHorizontalLines; ++i) {
        int y = plotRect.top() + (plotRect.height() * i) / numHorizontalLines;
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);

        // 数值标签
        double val = m_maxValue - (m_maxValue - m_minValue) * i / numHorizontalLines;
        painter.setPen(QColor(150, 150, 150));
        painter.drawText(5, y + 5, QString::number(val, 'f', 1));
        painter.setPen(gridPen);
    }

    // 边框
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(plotRect);
}

void WaveformWidget::drawWaveform(QPainter &painter, const QVector<DataPoint> &data,
                                   const QColor &color, const QString &label, int lineWidth, bool smooth)
{
    if (data.isEmpty()) return;

    QRect plotRect(m_marginLeft, m_marginTop,
                   width() - m_marginLeft - m_marginRight,
                   height() - m_marginTop - m_marginBottom);

    QPen pen(color);
    pen.setWidth(lineWidth);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);

    // 转换数据点到屏幕坐标
    QVector<QPointF> points;
    points.reserve(data.size());

    for (const auto &pt : data) {
        if (pt.timestamp < m_startTime || pt.timestamp > m_endTime) continue;

        double x = plotRect.left() + (pt.timestamp - m_startTime) * plotRect.width() /
                   static_cast<double>(m_endTime - m_startTime);
        double y = plotRect.top() + (m_maxValue - pt.value) * plotRect.height() /
                   (m_maxValue - m_minValue);
        y = std::clamp(y, static_cast<double>(plotRect.top()), static_cast<double>(plotRect.bottom()));

        points.append(QPointF(x, y));
    }

    if (points.size() > 1) {
        if (smooth && points.size() > 2) {
            // 使用贝塞尔曲线绘制平滑波形
            QPainterPath path(points.first());
            for (int i = 1; i < points.size() - 1; ++i) {
                QPointF midPoint((points[i].x() + points[i + 1].x()) / 2,
                                (points[i].y() + points[i + 1].y()) / 2);
                path.quadTo(points[i], midPoint);
            }
            path.lineTo(points.last());
            painter.drawPath(path);
        } else {
            painter.drawPolyline(points.data(), points.size());
        }
    }

    // 绘制图例
    painter.setPen(color);
    QFont legendFont = painter.font();
    legendFont.setBold(true);
    painter.setFont(legendFont);
    painter.drawText(plotRect.right() - 80, plotRect.top() + 20, label);
}

void WaveformWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void WaveformWidget::updateViewRange()
{
    if (m_rawData.isEmpty() && m_filteredData.isEmpty()) return;

    // 自动调整时间范围
    if (m_autoScroll) {
        quint64 maxTime = 0;
        if (!m_rawData.isEmpty()) {
            maxTime = std::max(maxTime, m_rawData.last().timestamp);
        }
        if (!m_filteredData.isEmpty()) {
            maxTime = std::max(maxTime, m_filteredData.last().timestamp);
        }

        m_endTime = maxTime;
        m_startTime = (maxTime > m_timeWindow) ? (maxTime - m_timeWindow) : 0;
    }

    // 自动调整数值范围
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();

    for (const auto &pt : m_rawData) {
        if (pt.timestamp >= m_startTime && pt.timestamp <= m_endTime) {
            minVal = std::min(minVal, pt.value);
            maxVal = std::max(maxVal, pt.value);
        }
    }

    for (const auto &pt : m_filteredData) {
        if (pt.timestamp >= m_startTime && pt.timestamp <= m_endTime) {
            minVal = std::min(minVal, pt.value);
            maxVal = std::max(maxVal, pt.value);
        }
    }

    if (minVal < maxVal) {
        // 添加 10% 边距
        double margin = (maxVal - minVal) * 0.1;
        m_minValue = minVal - margin;
        m_maxValue = maxVal + margin;
    }
}
