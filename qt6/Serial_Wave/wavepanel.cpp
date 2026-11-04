#include "wavepanel.h"
#include "qcustomplot.h"

#include <QVBoxLayout>
#include <QPen>
#include <QtMath>

WavePanel::WavePanel(QWidget *parent)
    : QWidget(parent)
    , m_plot(new QCustomPlot(this))
    , m_replotTimer(new QTimer(this))
    , m_time(0.0)
{
    // 1. 布局：QCustomPlot 就是一个普通 QWidget，直接塞进来
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_plot);

    // 2. QCustomPlot 初始化
    // 注意：Qt6 下 setOpenGl(true) 部分环境会黑屏/崩溃，先注释掉，
    // 性能不够再打开。打开时要放在 setInteractions 之前。
    // m_plot->setOpenGl(true);

    // QCustomPlot 2.x 没有 setAutoReplot：只要不调用 replot() 就不会重绘，
    // 重绘节奏完全由下面的定时器控制。
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // 3. 一条曲线
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(Qt::blue));

    // 4. 坐标轴初始范围
    m_plot->xAxis->setLabel("Time (s)");
    m_plot->yAxis->setLabel("Amplitude");
    m_plot->xAxis->setRange(0, 10);
    m_plot->yAxis->setRange(-1.5, 1.5);

    // 5. 定时重绘：约 33 FPS
    m_replotTimer->setInterval(30);
    connect(m_replotTimer, &QTimer::timeout,
            this, &WavePanel::onReplotTimeout);
    m_replotTimer->start();
}

void WavePanel::addData(double x, double y)
{
    // 只加数据 + 裁剪窗口，不在这里 replot
    m_plot->graph(0)->addData(x, y);

    // 滑动窗口：只保留最近 10 秒的数据
    // QCustomPlot 2.x 的删除接口在数据容器上，不在 QCPGraph 上
    m_plot->graph(0)->data()->removeBefore(x - 10.0);

    // 坐标轴跟着走
    m_plot->xAxis->setRange(x - 10.0, x);
}

void WavePanel::onReplotTimeout()
{
    // ===== 模拟数据，接真实数据后删掉这一段 =====
    m_time += 0.03;
    addData(m_time, qSin(m_time * 2 * M_PI));
    // =========================================

    // 定时器只负责重绘，排队重绘可以进一步节流
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}