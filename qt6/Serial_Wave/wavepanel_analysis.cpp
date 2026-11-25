#include "wavepanel.h"
#include "qcustomplot.h"
#include "logpanel.h"
#include <QtConcurrentRun>
#include <QBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <cmath>

namespace
{
QString valueText(double value)
{
    return qIsFinite(value) ? QString::number(value, 'f', 3) : QStringLiteral("--");
}
QString timeText(double value)
{
    return QString::number(value, 'f', 6);
}
const char *channelNames[] = {"破碎率 (%)", "含杂率 (%)", "实际转速 (rpm)",
                             "目标转速 (rpm)", "推杆行程 (%)"};
}

void WavePanel::setupAnalysisTools(QVBoxLayout *layout)
{
    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(8, 0, 8, 0);
    m_measureBtn = new QPushButton(tr("A/B 测量"), this);
    m_measureBtn->setObjectName("measureModeButton");
    m_measureBtn->setCheckable(true);
    m_measureBtn->setEnabled(false);
    m_measureBtn->setToolTip(tr("打开后在任意波形中依次单击 A、B；自动冻结并查询最近的原始已记录采样点。第三次单击重新选 A。"));
    auto *clear = new QPushButton(tr("清除测量"), this);
    clear->setObjectName("clearMeasurementButton");
    m_measureDetailsBtn = new QPushButton(tr("测量详情"), this);
    m_measureDetailsBtn->setObjectName("measurementDetailsButton");
    m_measureDetailsBtn->setEnabled(false);
    m_exportSelectionBtn = new QPushButton(tr("导出 A/B…"), this);
    m_exportSelectionBtn->setObjectName("exportSelectionButton");
    m_exportSelectionBtn->setEnabled(false);
    auto *whole = new QPushButton(tr("查看全程"), this);
    whole->setObjectName("wholeSessionButton");
    m_jumpTime = new QDoubleSpinBox(this);
    m_jumpTime->setObjectName("jumpTimeInput");
    m_jumpTime->setRange(0, 1e12);
    m_jumpTime->setDecimals(3);
    m_jumpTime->setSuffix(tr(" 秒"));
    m_jumpTime->setToolTip(tr("输入本次会话的主机接收时间；跳转后冻结显示，记录继续。"));
    auto *jump = new QPushButton(tr("跳转"), this);
    jump->setObjectName("jumpTimeButton");
    for (auto *button : {m_measureBtn, clear, m_measureDetailsBtn, m_exportSelectionBtn, whole})
        bar->addWidget(button);
    bar->addStretch();
    bar->addWidget(new QLabel(tr("定位时间"), this));
    bar->addWidget(m_jumpTime);
    bar->addWidget(jump);
    layout->addLayout(bar);
    connect(m_measureBtn, &QPushButton::toggled, this, [this](bool enabled)
    {
        for (auto *plot : m_plots)
            if (plot) plot->setInteraction(QCP::iRangeDrag, !enabled);
        if (enabled) m_pauseBtn->setChecked(true);
        updateAnalysisControls();
    });
    connect(clear, &QPushButton::clicked, this, &WavePanel::clearMeasurement);
    connect(m_measureDetailsBtn, &QPushButton::clicked, this, &WavePanel::showMeasurementDetails);
    connect(m_exportSelectionBtn, &QPushButton::clicked, this, &WavePanel::exportSelectionDialog);
    connect(whole, &QPushButton::clicked, this, &WavePanel::showWholeSession);
    connect(jump, &QPushButton::clicked, this, [this] { jumpToTime(m_jumpTime->value()); });
    connect(&m_measureWatcher, &QFutureWatcher<MeasurementData>::finished, this, [this]
    {
        m_measureBusy = false;
        if (m_loadingMeasureGeneration == m_measureGeneration)
        {
            m_measurement = m_measureWatcher.result();
            if (m_measurement.error.isEmpty())
            {
                if (m_measurement.hasA) m_measureARequest = m_measurement.a.time;
                if (m_measurement.hasB) m_measureBRequest = m_measurement.b.time;
            }
            else LogPanel::warning(tr("测量读取失败：%1").arg(m_measurement.error));
            refreshMeasurementDetails();
            m_dirty = true;
        }
        if (m_measurePending) loadMeasurement();
        updateAnalysisControls();
    });
}

void WavePanel::createMeasurementItems()
{
    const QColor colors[] = {QColor(80, 210, 255), QColor(255, 180, 85)};
    for (int p = 0; p < waveformPlotCount; ++p)
    {
        auto *plot = m_plots[p];
        auto *band = new QCPItemRect(plot);
        band->setLayer("grid");
        band->setBrush(QColor(90, 180, 235, 22));
        band->setPen(Qt::NoPen);
        for (auto *position : {band->topLeft, band->bottomRight})
        {
            position->setTypeX(QCPItemPosition::ptPlotCoords);
            position->setTypeY(QCPItemPosition::ptAxisRectRatio);
        }
        band->setVisible(false);
        m_measureBands[p] = band;
        for (int i = 0; i < 2; ++i)
        {
            auto *line = new QCPItemLine(plot);
            line->setLayer("overlay");
            line->setPen(QPen(colors[i], 1.2, Qt::DashLine));
            for (auto *position : {line->start, line->end})
            {
                position->setTypeX(QCPItemPosition::ptPlotCoords);
                position->setTypeY(QCPItemPosition::ptAxisRectRatio);
            }
            line->setVisible(false);
            m_measureLines[p][i] = line;
            auto *label = new QCPItemText(plot);
            label->setLayer("overlay");
            label->position->setTypeX(QCPItemPosition::ptPlotCoords);
            label->position->setTypeY(QCPItemPosition::ptAxisRectRatio);
            label->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
            label->setColor(colors[i]);
            label->setText(i == 0 ? "A" : "B");
            label->setVisible(false);
            m_measureLabels[p][i] = label;
        }
    }
}

void WavePanel::updateMeasurementItems()
{
    for (int p = 0; p < waveformPlotCount; ++p)
    {
        for (int i = 0; i < 2; ++i)
        {
            const auto &time = i == 0 ? m_measureARequest : m_measureBRequest;
            m_measureLines[p][i]->setVisible(time.has_value());
            m_measureLabels[p][i]->setVisible(time.has_value());
            if (!time) continue;
            m_measureLines[p][i]->start->setCoords(*time, 0);
            m_measureLines[p][i]->end->setCoords(*time, 1);
            m_measureLabels[p][i]->position->setCoords(*time, .02 + i * .07);
        }
        m_measureBands[p]->setVisible(m_measureARequest && m_measureBRequest);
        if (m_measureARequest && m_measureBRequest)
        {
            m_measureBands[p]->topLeft->setCoords(qMin(*m_measureARequest, *m_measureBRequest), 0);
            m_measureBands[p]->bottomRight->setCoords(qMax(*m_measureARequest, *m_measureBRequest), 1);
        }
    }
}

bool WavePanel::selectMeasurementPoint(double time)
{
    if (!qIsFinite(time) || time < 0 || !sampleCount() || time > lastTime() + 1e-9)
    {
        m_cursorInfo->setText(tr("该时间没有已记录数据；请开始记录或选择已记录的时间区间。"));
        return false;
    }
    m_measureBtn->setChecked(true);
    m_pauseBtn->setChecked(true);
    if (!m_measureARequest || m_measureBRequest)
    {
        m_measureARequest = time;
        m_measureBRequest.reset();
    }
    else m_measureBRequest = time;
    m_cursorInfo->setText(tr("A/B 查询原始记录；移动鼠标仍可查看绘图保留点。"));
    m_measurement = {};
    requestMeasurementRead();
    return true;
}

void WavePanel::requestMeasurementRead()
{
    ++m_measureGeneration;
    if (m_measureCancel) m_measureCancel->store(true);
    m_measurePending = m_measureARequest.has_value();
    if (m_measurePending && !m_measureBusy) loadMeasurement();
    if (m_measureTable) m_measureTable->clearContents();
    m_dirty = true;
    updateAnalysisControls();
}

void WavePanel::loadMeasurement()
{
    if (!m_measurePending || !m_measureARequest || m_measureBusy) return;
    m_measurePending = false;
    if (!flushRecording())
    {
        m_measurement.error = tr("当前会话尚未成功保存，请恢复保存后重新测量。");
        return;
    }
    m_measureBusy = true;
    m_loadingMeasureGeneration = m_measureGeneration;
    m_measureSourcePath = bufferFileName();
    m_measureCancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = m_measureCancel;
    const auto path = m_measureSourcePath;
    const auto a = *m_measureARequest;
    const bool hasB = m_measureBRequest.has_value();
    const auto b = m_measureBRequest.value_or(0);
    m_measureWatcher.setFuture(QtConcurrent::run([path, a, hasB, b, cancel]
    { return readSessionMeasurement(path, a, hasB, b, cancel); }));
}

void WavePanel::clearMeasurement()
{
    ++m_measureGeneration;
    if (m_measureCancel) m_measureCancel->store(true);
    m_measurePending = false;
    m_measureARequest.reset(); m_measureBRequest.reset();
    m_measurement = {};
    if (m_measureDialog) m_measureDialog->hide();
    m_dirty = true;
    updateAnalysisControls();
}

void WavePanel::updateAnalysisControls()
{
    if (!m_measureBtn) return;
    m_measureBtn->setEnabled(sampleCount() > 0);
    const bool ready = !m_measureBusy && !m_measurePending && m_measurement.error.isEmpty();
    m_measureDetailsBtn->setEnabled(ready && m_measurement.hasA);
    m_exportSelectionBtn->setEnabled(ready && m_measurement.hasB && !m_exportBusy);
    if (m_measureTable)
    {
        m_measureTable->setEnabled(ready && m_measurement.hasA);
        if (!ready)
            m_measureDetailsInfo->setText(m_measureBusy || m_measurePending
                ? tr("正在查询原始记录…") : m_measurement.error);
    }
    if (!m_measureInfo) return;
    m_measureInfo->setVisible(m_measureBtn->isChecked() || m_measureARequest.has_value());
    QString text;
    if (m_measureBusy || m_measurePending) text = tr("正在查询 A/B 原始记录…");
    else if (!m_measurement.error.isEmpty()) text = tr("测量读取失败：%1").arg(m_measurement.error);
    else if (m_measurement.hasB)
        text = tr("原始采样 A=%1 s    B=%2 s    Δt(B−A)=%3 s    区间 %4 点；详情包含数值差和全部原始区间统计。")
            .arg(timeText(m_measurement.a.time), timeText(m_measurement.b.time),
                 timeText(m_measurement.b.time - m_measurement.a.time)).arg(m_measurement.count);
    else if (m_measurement.hasA)
        text = tr("原始采样 A=%1 s；再单击波形选择 B。关闭 A/B 测量可恢复拖动。")
            .arg(timeText(m_measurement.a.time));
    else text = tr("A/B 测量：依次单击两个已记录时间点；第三次单击重新选择 A。采集和保存继续。关闭测量可恢复拖动。" );
    m_measureInfo->setText(text);
}

void WavePanel::showWholeSession()
{
    const double end = sampleCount() ? lastTime() : m_recent.empty() ? 0 : m_recent.back().time;
    if (!sampleCount() && m_recent.empty())
    {
        m_cursorInfo->setText(tr("当前没有可查看的数据。"));
        return;
    }
    const double start = sampleCount() ? 0 : m_recent.front().time;
    m_pauseBtn->setChecked(true);
    setRange(start, qMax(start + 1e-3, end));
    requestView();
    m_dirty = true;
}

void WavePanel::jumpToTime(double time)
{
    const double end = qMax(lastTime(), m_recent.empty() ? 0 : m_recent.back().time);
    if (!qIsFinite(time) || time < 0 || time > end || (!sampleCount() && m_recent.empty()))
    {
        m_cursorInfo->setText(tr("定位时间超出当前已有数据范围（0～%1 秒）。").arg(valueText(end)));
        return;
    }
    m_pauseBtn->setChecked(true);
    const double left = qMax(0.0, time - m_windowSec * .5);
    setRange(left, left + m_windowSec);
    requestView();
    m_dirty = true;
}

void WavePanel::refreshMeasurementDetails()
{
    if (!m_measureTable) return;
    QString text = tr("原始记录 A=%1 s").arg(timeText(m_measurement.a.time));
    if (m_measurement.hasB)
        text += tr("，B=%1 s，Δt(B−A)=%2 s；闭区间原始记录 %3 点，各通道仅统计有效值。")
            .arg(timeText(m_measurement.b.time), timeText(m_measurement.b.time - m_measurement.a.time))
            .arg(m_measurement.count);
    else text += tr("；请选择 B 查看差值和区间统计。");
    m_measureDetailsInfo->setText(text);
    for (int c = 0; c < waveformChannelCount; ++c)
    {
        const auto &s = m_measurement.stats[c];
        const double a = m_measurement.a.values[c], b = m_measurement.b.values[c];
        const QStringList row{tr(channelNames[c]), valueText(a),
            m_measurement.hasB ? valueText(b) : "--",
            m_measurement.hasB && qIsFinite(a) && qIsFinite(b) ? valueText(b - a) : "--",
            s.count ? valueText(s.min) : "--", s.count ? valueText(s.max) : "--",
            s.count ? valueText(s.mean) : "--",
            s.count ? valueText(std::sqrt(qMax(0.0, s.m2 / double(s.count)))) : "--",
            QString::number(s.count)};
        for (int col = 0; col < row.size(); ++col)
            m_measureTable->setItem(c, col, new QTableWidgetItem(row[col]));
    }
}

void WavePanel::showMeasurementDetails()
{
    if (!m_measurement.hasA || m_measureBusy || m_measurePending) return;
    if (!m_measureDialog)
    {
        m_measureDialog = new QDialog(this);
        m_measureDialog->setWindowTitle(tr("A/B 原始采样测量"));
        m_measureDialog->resize(1050, 340);
        auto *layout = new QVBoxLayout(m_measureDialog);
        m_measureDetailsInfo = new QLabel(m_measureDialog);
        m_measureDetailsInfo->setWordWrap(true);
        layout->addWidget(m_measureDetailsInfo);
        m_measureTable = new QTableWidget(waveformChannelCount, 9, m_measureDialog);
        m_measureTable->setObjectName("measurementTable");
        m_measureTable->setHorizontalHeaderLabels({tr("通道"), "A", "B", tr("差值 B−A"),
            tr("最小值"), tr("最大值"), tr("均值"), tr("总体标准差"), tr("有效点数")});
        m_measureTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_measureTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_measureTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_measureTable->setColumnWidth(0, 150);
        m_measureTable->verticalHeader()->hide();
        layout->addWidget(m_measureTable);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, m_measureDialog);
        buttons->button(QDialogButtonBox::Close)->setText(tr("关闭"));
        connect(buttons, &QDialogButtonBox::rejected, m_measureDialog, &QDialog::hide);
        layout->addWidget(buttons);
    }
    refreshMeasurementDetails();
    m_measureDialog->show(); m_measureDialog->raise();
}

void WavePanel::exportSelectionDialog()
{
    if (!m_measurement.hasB || m_measureBusy || m_measurePending || !m_measurement.error.isEmpty()) return;
    chooseExportPath(std::array<double, 2>{qMin(m_measurement.a.time, m_measurement.b.time),
                                          qMax(m_measurement.a.time, m_measurement.b.time)});
}
