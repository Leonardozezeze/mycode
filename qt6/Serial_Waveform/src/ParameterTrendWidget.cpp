#include "ParameterTrendWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <algorithm>
#include <limits>

ParameterTrendWidget::ParameterTrendWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(600, 400);
    setAutoFillBackground(true);
    setStyleSheet("background-color: #1e1e1e;");
}

ParameterTrendWidget::~ParameterTrendWidget() = default;

void ParameterTrendWidget::addDataPoint(quint64 timestamp, double lossRate, double fanRpm)
{
    m_data.append({timestamp, lossRate, fanRpm});

    // 限制数据点数量
    const int maxPoints = 50000;
    if (m_data.size() > maxPoints) {
        m_data.remove(0, m_data.size() - maxPoints);
    }

    updateViewRange();
    update();
}

void ParameterTrendWidget::clear()
{
    m_data.clear();
    m_startTime = 0;
    m_endTime = m_timeWindow;
    update();
}

void ParameterTrendWidget::setTimeWindow(quint64 ms)
{
    m_timeWindow = ms;
    updateViewRange();
    update();
}

void ParameterTrendWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    drawGrid(painter);
    drawLossRateCurve(painter);
    drawFanRpmCurve(painter);
}

void ParameterTrendWidget::drawGrid(QPainter &painter)
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
        painter.drawText(x - 25, plotRect.bottom() + 20, QString::number(t / 1000) + "s");
        painter.setPen(gridPen);
    }

    // 水平网格线
    const int numHorizontalLines = 8;
    for (int i = 0; i <= numHorizontalLines; ++i) {
        int y = plotRect.top() + (plotRect.height() * i) / numHorizontalLines;
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);

        // 左侧标签（损失率）
        double lossRate = m_maxLossRate - (m_maxLossRate - m_minLossRate) * i / numHorizontalLines;
        painter.setPen(QColor(244, 67, 54));  // 红色
        painter.drawText(5, y + 5, QString::number(lossRate, 'f', 1) + "%");

        // 右侧标签（转速）
        double fanRpm = m_maxFanRpm - (m_maxFanRpm - m_minFanRpm) * i / numHorizontalLines;
        painter.setPen(QColor(156, 39, 176));  // 紫色
        painter.drawText(plotRect.right() + 5, y + 5, QString::number(fanRpm, 'f', 0));
    }

    // 边框
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(plotRect);

    // 标题
    painter.setPen(QColor(200, 200, 200));
    QFont titleFont = painter.font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(width() / 2 - 80, 20, "参数趋势图");

    // Y轴标签
    painter.setPen(QColor(244, 67, 54));
    painter.drawText(5, m_marginTop - 10, "破碎率(%)");
    painter.setPen(QColor(156, 39, 176));
    painter.drawText(width() - m_marginRight + 5, m_marginTop - 10, "转速(RPM)");
}

void ParameterTrendWidget::drawLossRateCurve(QPainter &painter)
{
    if (m_data.isEmpty()) return;

    QRect plotRect(m_marginLeft, m_marginTop,
                   width() - m_marginLeft - m_marginRight,
                   height() - m_marginTop - m_marginBottom);

    QPen pen(QColor(244, 67, 54, 220));  // 红色
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    QVector<QPointF> points;
    points.reserve(m_data.size());

    for (const auto &pt : m_data) {
        if (pt.timestamp < m_startTime || pt.timestamp > m_endTime) continue;

        double x = plotRect.left() + (pt.timestamp - m_startTime) * plotRect.width() /
                   static_cast<double>(m_endTime - m_startTime);
        double y = plotRect.top() + (m_maxLossRate - pt.lossRate) * plotRect.height() /
                   (m_maxLossRate - m_minLossRate);
        y = std::clamp(y, static_cast<double>(plotRect.top()), static_cast<double>(plotRect.bottom()));

        points.append(QPointF(x, y));
    }

    if (points.size() > 2) {
        QPainterPath path(points.first());
        for (int i = 1; i < points.size() - 1; ++i) {
            QPointF midPoint((points[i].x() + points[i + 1].x()) / 2,
                            (points[i].y() + points[i + 1].y()) / 2);
            path.quadTo(points[i], midPoint);
        }
        path.lineTo(points.last());
        painter.drawPath(path);
    }
}

void ParameterTrendWidget::drawFanRpmCurve(QPainter &painter)
{
    if (m_data.isEmpty()) return;

    QRect plotRect(m_marginLeft, m_marginTop,
                   width() - m_marginLeft - m_marginRight,
                   height() - m_marginTop - m_marginBottom);

    QPen pen(QColor(156, 39, 176, 220));  // 紫色
    pen.setWidth(2);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);

    QVector<QPointF> points;
    points.reserve(m_data.size());

    for (const auto &pt : m_data) {
        if (pt.timestamp < m_startTime || pt.timestamp > m_endTime) continue;

        double x = plotRect.left() + (pt.timestamp - m_startTime) * plotRect.width() /
                   static_cast<double>(m_endTime - m_startTime);
        double y = plotRect.top() + (m_maxFanRpm - pt.fanRpm) * plotRect.height() /
                   (m_maxFanRpm - m_minFanRpm);
        y = std::clamp(y, static_cast<double>(plotRect.top()), static_cast<double>(plotRect.bottom()));

        points.append(QPointF(x, y));
    }

    if (points.size() > 2) {
        QPainterPath path(points.first());
        for (int i = 1; i < points.size() - 1; ++i) {
            QPointF midPoint((points[i].x() + points[i + 1].x()) / 2,
                            (points[i].y() + points[i + 1].y()) / 2);
            path.quadTo(points[i], midPoint);
        }
        path.lineTo(points.last());
        painter.drawPath(path);
    }

    // 图例
    painter.setPen(QColor(244, 67, 54));
    painter.drawText(plotRect.left() + 10, plotRect.top() + 20, "● 破碎率");
    painter.setPen(QColor(156, 39, 176));
    painter.drawText(plotRect.left() + 100, plotRect.top() + 20, "● 转速");
}

void ParameterTrendWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void ParameterTrendWidget::updateViewRange()
{
    if (m_data.isEmpty()) return;

    // 自动调整时间范围
    if (m_autoScroll) {
        quint64 maxTime = m_data.last().timestamp;
        m_endTime = maxTime;
        m_startTime = (maxTime > m_timeWindow) ? (maxTime - m_timeWindow) : 0;
    }

    // 自动调整数值范围
    double minLoss = std::numeric_limits<double>::max();
    double maxLoss = std::numeric_limits<double>::lowest();
    double minRpm = std::numeric_limits<double>::max();
    double maxRpm = std::numeric_limits<double>::lowest();

    for (const auto &pt : m_data) {
        if (pt.timestamp >= m_startTime && pt.timestamp <= m_endTime) {
            minLoss = std::min(minLoss, pt.lossRate);
            maxLoss = std::max(maxLoss, pt.lossRate);
            minRpm = std::min(minRpm, pt.fanRpm);
            maxRpm = std::max(maxRpm, pt.fanRpm);
        }
    }

    if (minLoss < maxLoss) {
        double margin = (maxLoss - minLoss) * 0.15;
        m_minLossRate = minLoss - margin;
        m_maxLossRate = maxLoss + margin;
    }

    if (minRpm < maxRpm) {
        double margin = (maxRpm - minRpm) * 0.15;
        m_minFanRpm = minRpm - margin;
        m_maxFanRpm = maxRpm + margin;
    }
}
