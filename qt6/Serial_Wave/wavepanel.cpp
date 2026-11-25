#include "wavepanel.h"
#include "qcustomplot.h"
#include "theme.h"
#include "logpanel.h"
#include "sessioncleanup.h"
#include <QLocale>
#include <QtConcurrentRun>
#include <QBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSplitter>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QMessageBox>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QUuid>
#include <QDialog>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QInputDialog>
#include <QTableView>
#include <QHeaderView>
#include <QStandardItemModel>
#include <cmath>
#include <algorithm>

namespace
{
constexpr const char *names[] = {"破碎率", "含杂率", "实际转速", "目标转速", "推杆行程"};
constexpr const char *axisNames[] = {"品质 (%)", "转速 (rpm)", "推杆行程 (%)"};
constexpr double defaultYMax[] = {20, 3000, 100};
const QColor colors[] = {QColor(70, 220, 150), QColor(255, 191, 75), QColor(75, 175, 255),
                         QColor(230, 145, 245), QColor(255, 140, 100)};
QString number(double v, int decimals = 2)
{
    return qIsFinite(v) ? QString::number(v, 'f', decimals) : QStringLiteral("--");
}
} // namespace

WavePanel::WavePanel(QWidget *parent)
    : QWidget(parent), m_replotTimer(new QTimer(this)), m_saveTimer(new QTimer(this)),
      m_viewTimer(new QTimer(this)), m_archive(sampleChannelCount)
{
    QSettings settings;
    m_recordingDirectory =
        settings
            .value("recording/directory",
                   QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                       "/sessions")
            .toString();
    m_eventModel = new QStandardItemModel(this);
    m_eventModel->setHorizontalHeaderLabels({tr("时间 (s)"), tr("类型"), tr("事件内容")});
    m_windowSec = settings.value("wave/windowSeconds", 10).toDouble();
    if (m_windowSec != 5 && m_windowSec != 10 && m_windowSec != 30 && m_windowSec != 60)
        m_windowSec = 10;
    m_autoY = settings.value("wave/autoY", true).toBool();
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *controls = new QFrame(this);
    controls->setProperty("workspaceCard", true);
    auto *controlsLayout = new QVBoxLayout(controls);
    controlsLayout->setContentsMargins(8, 3, 8, 3);
    controlsLayout->setSpacing(3);
    auto *bar = new QHBoxLayout;
    bar->setSpacing(6);
    auto *title = new QLabel(tr("波形对比"), this);
    title->setObjectName("panelTitle");
    bar->addWidget(title);
    m_pauseBtn = new QPushButton(tr("冻结显示"), this);
    m_pauseBtn->setCheckable(true);
    m_pauseBtn->setToolTip(tr("冻结绘图以查看历史，采集与保存继续进行。拖动波形也会自动冻结。"));
    m_autoYBtn = new QPushButton(tr("Y 自适应"), this);
    m_autoYBtn->setCheckable(true);
    m_autoYBtn->setChecked(m_autoY);
    m_windowCombo = new QComboBox(this);
    for (int seconds : {5, 10, 30, 60})
        m_windowCombo->addItem(tr("%1 秒").arg(seconds), seconds);
    m_windowCombo->setCurrentIndex(m_windowCombo->findData(int(m_windowSec)));
    auto *latest = new QPushButton(tr("回到最新"), this);
    auto *stats = new QPushButton(tr("区间统计"), this);
    stats->setToolTip(tr("统计当前时间范围内的原始已记录数据，包含全部采样点"));
    auto *png = new QPushButton(tr("导出 PNG"), this);
    auto *channels = new QPushButton(tr("通道与坐标"), this);
    channels->setObjectName("channelSettingsButton");
    connect(channels, &QPushButton::clicked, this, &WavePanel::configureChannels);
    for (auto *button : {m_pauseBtn, latest})
        bar->addWidget(button);
    bar->addWidget(m_windowCombo);
    bar->addWidget(m_autoYBtn);
    m_sessionInfo = new QLabel(tr("连接设备后自动开始记录，也可手动开始"), this);
    m_sessionInfo->setObjectName("hintText");
    m_sessionInfo->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_sessionInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    bar->addWidget(m_sessionInfo, 1);
    controlsLayout->addLayout(bar);

    auto *sessionBar = new QHBoxLayout;
    sessionBar->setSpacing(6);
    m_openSessionBtn = new QPushButton(tr("打开会话…"), this);
    sessionBar->addWidget(channels);
    sessionBar->addWidget(stats);
    sessionBar->addWidget(png);
    sessionBar->addStretch();
    m_noteBtn = new QPushButton(tr("添加备注"), this);
    m_noteBtn->setObjectName("addSessionNoteButton");
    m_noteBtn->setEnabled(false);
    m_eventsBtn = new QPushButton(tr("事件 (0)"), this);
    m_eventsBtn->setObjectName("sessionEventsButton");
    sessionBar->addWidget(m_noteBtn);
    sessionBar->addWidget(m_eventsBtn);
    sessionBar->addWidget(m_openSessionBtn);
    connect(m_noteBtn, &QPushButton::clicked, this, &WavePanel::addNoteDialog);
    connect(m_eventsBtn, &QPushButton::clicked, this, &WavePanel::showEvents);
    controlsLayout->addLayout(sessionBar);
    setupAnalysisTools(controlsLayout);
    layout->addWidget(controls);
    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->setObjectName("waveSplitter");
    splitter->setChildrenCollapsible(false);
    for (int p = 0; p < waveformPlotCount; ++p)
    {
        auto *plot = new QCustomPlot(splitter);
        plot->setObjectName(p == 0 ? "qualityPlot" : p == 1 ? "speedPlot" : "actuatorPlot");
        m_plots[p] = plot;
        plot->setMinimumHeight(90);
        plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        plot->axisRect()->setRangeZoom(Qt::Horizontal);
        plot->axisRect()->setRangeDrag(Qt::Horizontal);
        plot->setBackground(QBrush(THEME_BG));
        plot->axisRect()->setBackground(QBrush(THEME_BG));
        for (auto *axis : {plot->xAxis, plot->yAxis})
        {
            axis->setBasePen(QPen(THEME_AXIS));
            axis->setTickPen(QPen(THEME_AXIS));
            axis->setSubTickPen(QPen(THEME_AXIS));
            axis->setTickLabelColor(THEME_TEXT);
            axis->setLabelColor(THEME_TEXT);
            axis->grid()->setPen(QPen(THEME_GRID, 1, Qt::DotLine));
            axis->grid()->setZeroLinePen(QPen(THEME_ZERO_LINE));
        }
        plot->yAxis->setLabel(tr(axisNames[p]));
        if (p == waveformPlotCount - 1)
            plot->xAxis->setLabel(tr("主机接收时间 (s)"));
        else
            plot->xAxis->setTickLabels(false);
        plot->xAxis->setRange(0, m_windowSec);
        const double lo = settings.value(QString("wave/y%1Min").arg(p), 0).toDouble();
        const double hi = settings.value(QString("wave/y%1Max").arg(p), defaultYMax[p]).toDouble();
        if (qIsFinite(lo) && qIsFinite(hi) && hi > lo && QCPRange::validRange(lo, hi))
            m_fixedY[p] = {lo, hi};
        plot->yAxis->setRange(m_fixedY[p][0], m_fixedY[p][1]);
        connect(plot->yAxis, qOverload<const QCPRange &>(&QCPAxis::rangeChanged), this,
                [this, p](const QCPRange &range)
                {
                    if (!m_autoY)
                    {
                        m_fixedY[p] = {range.lower, range.upper};
                        QSettings settings;
                        settings.setValue(QString("wave/y%1Min").arg(p), range.lower);
                        settings.setValue(QString("wave/y%1Max").arg(p), range.upper);
                    }
                });
        plot->legend->setVisible(true);
        plot->legend->setBrush(QBrush(QColor(35, 35, 35, 220)));
        plot->legend->setTextColor(THEME_TEXT);
        plot->legend->setBorderPen(Qt::NoPen);
        plot->legend->setFont(QFont(font().family(), 9));
        plot->legend->setFillOrder(QCPLegend::foColumnsFirst);
        plot->legend->setWrap(2);
        for (int c = 0; c < plotGraphCounts[p]; ++c)
        {
            auto *g = plot->addGraph();
            g->setName(tr(names[p * 2 + c]));
            const int channel = p * 2 + c;
            QColor color(settings.value(QString("wave/channel%1Color").arg(channel), colors[channel]).value<QColor>());
            if (!color.isValid())
                color = colors[channel];
            g->setPen(QPen(color, 1.5, c == 1 && p == 1 ? Qt::DashLine : Qt::SolidLine));
            const bool visible = settings.value(QString("wave/channel%1Visible").arg(channel), true).toBool();
            g->setVisible(visible);
            plot->legend->itemWithPlottable(g)->setTextColor(visible ? QColor(THEME_TEXT) : QColor(100, 100, 100));
        }
        for (int i = 0; i < 32; ++i)
        {
            auto *marker = new QCPItemLine(plot);
            marker->setPen(QPen(QColor(190, 170, 100, 150), 1, Qt::DotLine));
            for (auto *position : {marker->start, marker->end})
            {
                position->setTypeX(QCPItemPosition::ptPlotCoords);
                position->setTypeY(QCPItemPosition::ptAxisRectRatio);
            }
            marker->setVisible(false);
            m_eventLines[p].append(marker);
            if (p == 0)
            {
                auto *label = new QCPItemText(plot);
                label->position->setTypeX(QCPItemPosition::ptPlotCoords);
                label->position->setTypeY(QCPItemPosition::ptAxisRectRatio);
                label->setPositionAlignment(Qt::AlignTop | Qt::AlignLeft);
                label->setColor(QColor(220, 200, 140));
                label->setFont(QFont(font().family(), 8));
                label->setVisible(false);
                m_eventLabels.append(label);
            }
        }
        auto *line = new QCPItemLine(plot);
        m_cursors[p] = line;
        line->setPen(QPen(QColor(180, 180, 180), 1, Qt::DashLine));
        line->setVisible(false);
        connect(plot->xAxis, qOverload<const QCPRange &>(&QCPAxis::rangeChanged), this,
                [this](const QCPRange &range)
                {
                    if (m_loadingView)
                        return;
                    setRange(range.lower, range.upper);
                    m_pauseBtn->setChecked(true);
                    m_dirty = true;
                    requestView();
                });
        connect(plot, &QCustomPlot::mouseMove, this,
                [this, plot](QMouseEvent *event)
                {
                    if (!plot->axisRect()->rect().contains(event->pos()))
                        return;
                    const double x = plot->xAxis->pixelToCoord(event->pos().x());
                    if (!plot->graph(0)->dataCount())
                        return;
                    auto it = plot->graph(0)->data()->findBegin(x);
                    if (it == plot->graph(0)->data()->constEnd())
                        --it;
                    else if (it != plot->graph(0)->data()->constBegin() &&
                             std::abs((it - 1)->key - x) < std::abs(it->key - x))
                        --it;
                    const double t = it->key;
                    QStringList values;
                    for (int p = 0; p < waveformPlotCount; ++p)
                    {
                        for (int c = 0; c < plotGraphCounts[p]; ++c)
                        {
                            const auto data = m_plots[p]->graph(c)->data();
                            auto v = data->findBegin(t);
                            values << (v != data->constEnd() ? number(v->value)
                                                             : QStringLiteral("--"));
                        }
                        m_cursors[p]->start->setCoords(t, m_plots[p]->yAxis->range().lower);
                        m_cursors[p]->end->setCoords(t, m_plots[p]->yAxis->range().upper);
                        m_cursors[p]->setVisible(true);
                        m_plots[p]->replot(QCustomPlot::rpQueuedReplot);
                    }
                    m_cursorInfo->setText(
                        tr("t=%1 s    破碎率 %2%    含杂率 %3%    实际 %4 rpm    目标 %5 rpm    推杆 %6%")
                            .arg(number(t, 3), values[0], values[1], values[2], values[3], values[4]));
                });
        connect(plot, &QCustomPlot::mouseWheel, this,
                [this, plot](QWheelEvent *event)
                {
                    const bool vertical = event->modifiers().testFlag(Qt::ControlModifier);
                    plot->axisRect()->setRangeZoom(vertical ? Qt::Vertical : Qt::Horizontal);
                    if (vertical)
                        m_autoYBtn->setChecked(false);
                });
        connect(plot, &QCustomPlot::mousePress, this,
                [this, plot](QMouseEvent *event)
                {
                    if (m_measureBtn->isChecked())
                    {
                        if (event->button() == Qt::LeftButton && plot->axisRect()->rect().contains(event->pos()))
                            selectMeasurementPoint(plot->xAxis->pixelToCoord(event->pos().x()));
                        return;
                    }
                    const bool vertical = event->modifiers().testFlag(Qt::ShiftModifier);
                    plot->axisRect()->setRangeDrag(vertical ? Qt::Vertical : Qt::Horizontal);
                    if (vertical)
                    {
                        m_autoYBtn->setChecked(false);
                        m_pauseBtn->setChecked(true);
                    }
                });
    }
    auto *axisMargins = new QCPMarginGroup(m_plots[0]);
    for (auto *plot : m_plots)
        plot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, axisMargins);
    createMeasurementItems();
    layout->addWidget(splitter, 1);
    for (int p = 0; p < waveformPlotCount; ++p)
        splitter->setStretchFactor(p, 1);
    m_cursorInfo =
        new QLabel(tr("移动鼠标查看采样值；拖动或滚轮缩放以回看；时间使用主机接收时钟。"), this);
    m_cursorInfo->setObjectName("hintText");
    m_cursorInfo->setWordWrap(true);
    m_cursorInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_measureInfo = new QLabel(this);
    m_measureInfo->setObjectName("measurementInfo");
    m_measureInfo->setWordWrap(true);
    m_measureInfo->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_measureInfo->setVisible(false);
    layout->addWidget(m_measureInfo);
    layout->addWidget(m_cursorInfo);
    connect(m_pauseBtn, &QPushButton::toggled, this, &WavePanel::togglePause);
    connect(m_autoYBtn, &QPushButton::toggled, this, &WavePanel::toggleAutoY);
    connect(latest, &QPushButton::clicked, this,
            [this]
            {
                if (m_playback)
                {
                    setRange(qMax(0.0, lastTime() - m_windowSec), qMax(m_windowSec, lastTime()));
                    requestView();
                }
                else
                {
                    m_pauseBtn->setChecked(false);
                    followLatest();
                }
            });
    connect(m_windowCombo, &QComboBox::currentIndexChanged, this,
            [this]
            {
                m_windowSec = m_windowCombo->currentData().toDouble();
                QSettings().setValue("wave/windowSeconds", m_windowSec);
                if (!m_paused)
                    followLatest();
                else
                {
                    const double right = m_plots[0]->xAxis->range().upper;
                    setRange(qMax(0.0, right - m_windowSec), qMax(right, m_windowSec));
                    requestView();
                }
            });
    connect(stats, &QPushButton::clicked, this,
            [this]
            {
                m_showStats = true;
                m_pauseBtn->setChecked(true);
                requestView();
            });
    connect(png, &QPushButton::clicked, this, &WavePanel::exportPngDialog);
    connect(m_openSessionBtn, &QPushButton::clicked, this, &WavePanel::openSessionDialog);
    m_replotTimer->setInterval(30);
    connect(m_replotTimer, &QTimer::timeout, this, &WavePanel::onReplotTimeout);
    m_replotTimer->start();
    m_saveTimer->setInterval(1000);
    connect(m_saveTimer, &QTimer::timeout, this,
            [this]
            {
                if (m_recording)
                    flushRecording();
            });
    m_saveTimer->start();
    m_viewTimer->setSingleShot(true);
    m_viewTimer->setInterval(70);
    connect(m_viewTimer, &QTimer::timeout, this, &WavePanel::loadView);
    connect(&m_viewWatcher, &QFutureWatcher<ViewData>::finished, this,
            [this]
            {
                m_viewBusy = false;
                const auto result = m_viewWatcher.result();
                if (m_loadingGeneration == m_viewGeneration)
                {
                    if (!result.error.isEmpty())
                        LogPanel::error(tr("读取历史失败：%1").arg(result.error));
                    else
                    {
                        auto points = result.points;
                        if (!m_playback)
                        {
                            double last = result.archiveLastTime;
                            const auto range = m_plots[0]->xAxis->range();
                            for (const auto &p : m_recent)
                                if (p.time > last && p.time >= range.lower && p.time <= range.upper)
                                    points.append(p);
                        }
                        setPoints(points);
                        if (m_playback)
                            m_cursorInfo->setText(
                                tr("历史回看 · 游标显示保留采样点；区间统计使用全部原始记录。"));
                        applyAutoY();
                        m_dirty = true;
                        if (m_showStats)
                        {
                            m_showStats = false;
                            showStats(result);
                        }
                    }
                }
                if (m_viewPending)
                    m_viewTimer->start();
            });
    auto *progressTimer = new QTimer(this);
    progressTimer->setInterval(100);
    connect(progressTimer, &QTimer::timeout, this,
            [this]
            {
                if (m_exportProgress && m_exportState)
                    m_exportProgress->setValue(int(1000.0 * m_exportState->done.load() /
                                                   qMax(qint64(1), m_exportState->total)));
            });
    progressTimer->start();
}

WavePanel::~WavePanel()
{
    cancelExport();
    if (m_viewCancel)
        m_viewCancel->store(true);
    if (m_measureCancel) m_measureCancel->store(true);
    m_archive.flush();
    m_events.flush();
}

void WavePanel::startRecording()
{
    if (m_recording)
        return;
    if (!flushRecording())
        return;
    QDir dir(m_recordingDirectory);
    if (!dir.mkpath("."))
    {
        QMessageBox::warning(this, tr("无法开始记录"), tr("无法创建记录目录，请选择其他目录。"));
        return;
    }
    const QString name = QStringLiteral("session_%1_%2.swave")
                             .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"),
                                  QUuid::createUuid().toString(QUuid::Id128).left(8));
    if (!m_archive.create(dir.filePath(name), sampleChannelCount))
    {
        reportStorageError();
        return;
    }
    if (m_viewCancel)
        m_viewCancel->store(true);
    ++m_viewGeneration;
    m_viewPending = false;
    m_viewTimer->stop();
    clearMeasurement();
    m_measureBtn->setChecked(false);
    m_events.begin(bufferFileName(), startedAt());
    m_eventLoadError = false;
    m_eventClock.start();
    rebuildEventModel();
    m_recent.clear();
    m_playback = false;
    m_recording = true;
    m_storageError = false;
    m_showStats = false;
    for (auto *plot : m_plots)
        for (int c = 0; c < plot->graphCount(); ++c)
            plot->graph(c)->data()->clear();
    for (auto *cursor : m_cursors)
        cursor->setVisible(false);
    m_pauseBtn->setChecked(false);
    m_paused = false;
    setRange(0, m_windowSec);
    m_dirty = true;
    m_cursorInfo->setText(tr("记录已开始，等待数据；冻结显示不会停止记录。"));
    updateControls();
    emit sessionStarted();
    addSessionEvent("record_start", tr("开始记录"));
    emit sessionChanged();
    LogPanel::info(tr("新建采集会话：%1").arg(bufferFileName()));
}

bool WavePanel::stopRecording()
{
    if (m_recording)
        addSessionEvent("record_stop", tr("停止记录"));
    m_recording = false;
    const bool ok = flushRecording();
    updateControls();
    emit sessionChanged();
    return ok;
}

bool WavePanel::flushRecording()
{
    if (!m_archive.flush())
    {
        reportStorageError();
        return false;
    }
    if (!m_events.flush())
    {
        reportStorageError(tr("会话事件保存失败：%1").arg(m_events.errorString()));
        return false;
    }
    if (m_storageError)
    {
        m_storageError = false;
        updateControls();
        emit sessionChanged();
    }
    return true;
}

void WavePanel::reportStorageError(const QString &message)
{
    m_recording = false;
    const bool first = !m_storageError;
    m_storageError = true;
    updateControls();
    emit sessionChanged();
    if (first)
        emit storageFailed(message.isEmpty() ? m_archive.errorString() : message);
}

void WavePanel::addSample(const TelemetrySample &sample)
{
    const auto values = sample.values();
    addData(sample.time, QVector<double>(values.begin(), values.end()));
}

void WavePanel::addData(double x, const QVector<double> &values)
{
    if (m_playback || !qIsFinite(x) || x < 0)
        return;
    SamplePoint p;
    p.time = x;
    p.breakBefore = !m_recent.empty() && x - m_recent.back().time > 2.0;
    if (m_recent.empty())
        m_cursorInfo->setText(
            tr("移动鼠标查看采样值；拖动或滚轮回看；Ctrl+滚轮缩放 Y，Shift+拖动平移 Y。"));
    for (int c = 0; c < sampleChannelCount; ++c)
        p.values[c] = c < values.size() ? values[c] : qQNaN();
    m_recent.push_back(p);
    while (m_recent.size() > 60000 || (!m_recent.empty() && m_recent.front().time < x - 60))
        m_recent.pop_front();
    if (m_recording && !m_archive.append(x, p.values.data()))
        reportStorageError();
    if (sampleCount() == 1) updateAnalysisControls();
    if (m_paused)
        return;
    appendToGraphs(p);
    for (auto *plot : m_plots)
        for (int c = 0; c < plot->graphCount(); ++c)
        {
            auto data = plot->graph(c)->data();
            data->removeBefore(x - m_windowSec);
            if (data->size() > 60000)
                data->removeBefore((data->constEnd() - 60000)->key);
        }
    setRange(qMax(0.0, x - m_windowSec), qMax(m_windowSec, x));
    m_dirty = true;
}

void WavePanel::appendToGraphs(const SamplePoint &p)
{
    for (int i = 0; i < waveformPlotCount; ++i)
        for (int c = 0; c < plotGraphCounts[i]; ++c)
        {
            auto *graph = m_plots[i]->graph(c);
            if (graph->dataCount() && p.breakBefore)
                graph->addData((graph->data()->constEnd() - 1)->key + 1e-9, qQNaN());
            graph->addData(p.time, p.values[i * 2 + c]);
        }
}

void WavePanel::setPoints(const QVector<SamplePoint> &points)
{
    QVector<double> x;
    std::array<QVector<double>, waveformChannelCount> values;
    x.reserve(points.size());
    for (auto &v : values)
        v.reserve(points.size());
    double previous = -1;
    for (const auto &p : points)
    {
        if (previous >= 0 && p.breakBefore)
        {
            x.append(previous + 1e-9);
            for (auto &v : values)
                v.append(qQNaN());
        }
        x.append(p.time);
        for (int c = 0; c < waveformChannelCount; ++c)
            values[c].append(p.values[c]);
        previous = p.time;
    }
    for (int p = 0; p < waveformPlotCount; ++p)
        for (int c = 0; c < plotGraphCounts[p]; ++c)
            m_plots[p]->graph(c)->setData(x, values[p * 2 + c], true);
}

void WavePanel::setRange(double lo, double hi)
{
    if (!qIsFinite(lo) || !qIsFinite(hi))
        return;
    // 拖动、滚轮缩放和程序设置均走这里；越过起点时平移整个窗口，保留窗口宽度。
    const double span = qMax(hi - lo, 1e-6);
    lo = qMax(0.0, lo);
    hi = lo + span;
    m_loadingView = true;
    for (auto *plot : m_plots)
        plot->xAxis->setRange(lo, hi);
    m_loadingView = false;
}

void WavePanel::resetPlotLayout()
{
    if (auto *splitter = findChild<QSplitter *>("waveSplitter"))
    {
        splitter->setSizes({1, 1, 1});
    }
}

void WavePanel::followLatest()
{
    if (m_recent.empty())
        return;
    const double last = m_recent.back().time;
    const double left = qMax(0.0, last - m_windowSec);
    QVector<SamplePoint> points;
    for (const auto &p : m_recent)
        if (p.time >= left)
            points.append(p);
    setPoints(points);
    setRange(left, qMax(m_windowSec, last));
    m_dirty = true;
}

void WavePanel::togglePause(bool paused)
{
    if (m_playback && !paused)
    {
        m_pauseBtn->setChecked(true);
        return;
    }
    m_paused = paused;
    m_pauseBtn->setText(paused ? tr("恢复跟随") : tr("冻结显示"));
    if (!paused)
    {
        clearMeasurement();
        m_measureBtn->setChecked(false);
        if (m_viewCancel)
            m_viewCancel->store(true);
        ++m_viewGeneration;
        m_viewPending = false;
        m_viewTimer->stop();
        m_showStats = false;
        followLatest();
    }
}

void WavePanel::toggleAutoY(bool enabled)
{
    m_autoY = enabled;
    QSettings().setValue("wave/autoY", enabled);
    applyAutoY();
    m_dirty = true;
}

void WavePanel::applyAutoY()
{
    if (!m_autoY)
        return;
    for (int p = 0; p < waveformPlotCount; ++p)
    {
        auto *plot = m_plots[p];
        if (p == 2)
        {
            // 行程有明确物理范围，默认固定为 0～100%，避免曲线看起来超程。
            plot->yAxis->setRange(0, 100);
            continue;
        }
        bool visible = false;
        for (int c = 0; c < plot->graphCount(); ++c)
            visible |= plot->graph(c)->visible();
        if (!visible || !plot->graph(0)->dataCount())
            continue;
        plot->yAxis->rescale(true);
        const auto r = plot->yAxis->range();
        const double pad = qMax(r.size() * .1, p == 0 ? .1 : 1.0);
        plot->yAxis->setRange(r.lower - pad, r.upper + pad);
    }
}

void WavePanel::onReplotTimeout()
{
    if (!m_dirty)
        return;
    m_dirty = false;
    applyAutoY();
    updateEventMarkers();
    updateMeasurementItems();
    updateAnalysisControls();
    for (auto *plot : m_plots)
        plot->replot(QCustomPlot::rpQueuedReplot);
}

void WavePanel::requestView()
{
    ++m_viewGeneration;
    m_viewPending = true;
    if (m_viewCancel)
        m_viewCancel->store(true);
    m_viewTimer->start();
}

void WavePanel::loadView()
{
    if (!m_paused || !m_viewPending)
        return;
    if (m_viewBusy)
        return; // 完成槽会调度最新请求
    m_viewPending = false;
    const auto range = m_plots[0]->xAxis->range();
    if (bufferFileName().isEmpty() || !sampleCount())
    {
        QVector<SamplePoint> points;
        for (const auto &p : m_recent)
            if (p.time >= range.lower && p.time <= range.upper)
                points.append(p);
        setPoints(points);
        m_dirty = true;
        if (m_showStats)
        {
            m_showStats = false;
            QMessageBox::information(this, tr("区间统计"),
                                     tr("当前没有已记录数据。请先开始记录。"));
        }
        return;
    }
    if (!flushRecording())
        return;
    m_loadingGeneration = m_viewGeneration;
    m_viewBusy = true;
    m_viewCancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = m_viewCancel;
    const auto path = bufferFileName();
    m_viewSourcePath = path;
    m_viewWatcher.setFuture(QtConcurrent::run(
        [path, range, cancel] { return readSessionView(path, range.lower, range.upper, cancel); }));
}

void WavePanel::showStats(const ViewData &data)
{
    if (!data.count)
    {
        QMessageBox::information(this, tr("区间统计"), tr("当前时间范围没有已记录的采样点。"));
        return;
    }
    QString text = tr("当前区间：%1 ～ %2 秒\n原始采样点：%3\n\n")
                       .arg(number(m_plots[0]->xAxis->range().lower, 3),
                            number(m_plots[0]->xAxis->range().upper, 3))
                       .arg(data.count);
    for (int c = 0; c < waveformChannelCount; ++c)
    {
        const auto &s = data.stats[c];
        if (!s.count)
        {
            text += tr("%1：暂无有效数据\n").arg(tr(names[c]));
            continue;
        }
        text += tr("%1：均值 %2，最小 %3，最大 %4，标准差 %5\n")
                    .arg(tr(names[c]), number(s.mean), number(s.min), number(s.max),
                         number(std::sqrt(qMax(0.0, s.m2 / double(s.count)))));
    }
    QMessageBox::information(this, tr("区间统计（全部原始采样）"), text);
}

double WavePanel::lastTime() const
{
    double x = 0;
    return m_archive.lastX(x) ? x : 0;
}
void WavePanel::clearData()
{
    if (!stopRecording())
        return;
    startRecording();
}

bool WavePanel::openSession(const QString &path)
{
    if (m_connected)
    {
        LogPanel::warning(tr("请先断开传感连接，再打开历史会话。"));
        return false;
    }
    if (!stopRecording())
        return false;
    if (!m_archive.open(path))
    {
        LogPanel::error(m_archive.errorString());
        return false;
    }
    clearMeasurement();
    m_measureBtn->setChecked(false);
    m_eventLoadError = !m_events.load(path, startedAt());
    if (m_eventLoadError)
        LogPanel::warning(tr("波形已打开，但配套事件未载入：%1").arg(m_events.errorString()));
    else if (m_events.ignoredTail())
        LogPanel::warning(tr("已恢复完整事件，忽略事件文件末尾未写完的一行。"));
    rebuildEventModel();
    m_recent.clear();
    m_playback = true;
    m_storageError = false;
    m_pauseBtn->setChecked(true);
    for (auto *cursor : m_cursors)
        cursor->setVisible(false);
    for (auto *plot : m_plots)
        for (int c = 0; c < plot->graphCount(); ++c)
            plot->graph(c)->data()->clear();
    m_cursorInfo->setText(tr("正在加载历史会话…"));
    m_dirty = true;
    const double last = lastTime();
    setRange(qMax(0.0, last - m_windowSec), qMax(last, m_windowSec));
    updateControls();
    emit sessionChanged();
    requestView();
    if (m_archive.ignoredTailBytes())
        LogPanel::warning(tr("已恢复 %1 个完整采样点，忽略文件末尾 %2 个未写完字节。")
                              .arg(sampleCount())
                              .arg(m_archive.ignoredTailBytes()));
    return true;
}

void WavePanel::openSessionDialog()
{
    const auto path = QFileDialog::getOpenFileName(this, tr("打开采集会话"), m_recordingDirectory,
                                                   tr("采集会话 (*.swave)"));
    if (!path.isEmpty() && !openSession(path))
        QMessageBox::warning(this, tr("打开失败"),
                             m_connected ? tr("请先断开传感连接。") : m_archive.errorString());
}

void WavePanel::setConnected(bool connected)
{
    m_connected = connected;
    updateControls();
}
void WavePanel::updateControls()
{
    updateAnalysisControls();
    m_openSessionBtn->setEnabled(!m_connected);
    m_noteBtn->setEnabled(!m_playback && !bufferFileName().isEmpty() && !m_storageError);
    m_eventsBtn->setText(tr("事件 (%1)").arg(m_events.entries().size()));
    m_eventsBtn->setToolTip(m_eventLoadError ? tr("配套事件读取失败，请检查日志") :
        tr("双击事件跳转到对应时间；图中最多显示当前范围内最后 32 个事件。"));
    m_pauseBtn->setEnabled(!m_playback);
    m_pauseBtn->setText(m_playback ? tr("历史回看") : m_paused ? tr("恢复跟随") : tr("冻结显示"));
    const auto path = bufferFileName();
    const auto mode = m_storageError ? tr("保存失败，记录已停止")
                      : m_playback   ? tr("历史回看")
                      : m_recording  ? tr("记录中 · 每秒自动保存")
                                     : tr("记录已停止");
    m_sessionInfo->setText(path.isEmpty()
                               ? tr("连接传感设备后自动记录；或点击“开始记录”")
                               : mode + QStringLiteral(" · ") + QFileInfo(path).fileName());
    m_sessionInfo->setToolTip(path);
}

void WavePanel::chooseRecordingDirectory()
{
    const auto dir =
        QFileDialog::getExistingDirectory(this, tr("选择后续会话的保存目录"), m_recordingDirectory);
    if (!dir.isEmpty())
    {
        m_recordingDirectory = dir;
        QSettings().setValue("recording/directory", dir);
        LogPanel::info(tr("后续会话保存目录：%1").arg(dir));
    }
}
void WavePanel::showRecordingDirectory()
{
    const auto dir = bufferFileName().isEmpty() ? m_recordingDirectory
                                                : QFileInfo(bufferFileName()).absolutePath();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

bool WavePanel::exportDataTo(const QString &path)
{
    return startExport(path, std::nullopt);
}

bool WavePanel::exportRangeTo(const QString &path, double lower, double upper)
{
    return startExport(path, std::array<double, 2>{lower, upper});
}

bool WavePanel::startExport(const QString &path, const std::optional<std::array<double, 2>> &range)
{
    if (m_exportBusy)
    {
        LogPanel::warning(tr("已有导出任务正在进行。"));
        return false;
    }
    const auto suffix = QFileInfo(path).suffix().toLower();
    if (suffix != "csv" && suffix != "json")
    {
        LogPanel::warning(tr("请选择 CSV 或 JSON 导出格式。"));
        return false;
    }
    if (!sampleCount() || !flushRecording())
        return false;
    const qint64 points = range ? m_archive.countInRange((*range)[0], (*range)[1]) : sampleCount();
    if (points <= 0)
    {
        LogPanel::warning(points < 0 ? m_archive.errorString() : tr("选中区间没有已记录数据，未导出。"));
        return false;
    }
    m_exportBusy = true;
    updateAnalysisControls();
    m_exportState = std::make_shared<ExportState>();
    m_exportState->total = points;
    if (range)
    {
        m_exportState->selectedRange = true;
        m_exportState->lower = (*range)[0];
        m_exportState->upper = (*range)[1];
    }
    const auto state = m_exportState;
    const auto source = bufferFileName();
    m_exportSourcePath = source;
    m_exportProgress = new QProgressDialog(tr("正在导出 %1 个采样点…").arg(points),
                                           tr("取消"), 0, 1000, this);
    m_exportProgress->setWindowTitle(tr("导出数据"));
    m_exportProgress->setAutoClose(false);
    m_exportProgress->setAutoReset(false);
    m_exportProgress->setMinimumDuration(0);
    m_exportProgress->setValue(0);
    connect(m_exportProgress, &QProgressDialog::canceled, this,
            [state] { state->cancelled.store(true); });
    connect(&m_exportWatcher, &QFutureWatcher<ExportResult>::finished, this,
            [this, path]
            {
                const auto r = m_exportWatcher.result();
                if (m_exportProgress)
                {
                    m_exportProgress->hide();
                    m_exportProgress->deleteLater();
                    m_exportProgress = nullptr;
                }
                disconnect(&m_exportWatcher, &QFutureWatcher<ExportResult>::finished, this,
                           nullptr);
                m_exportBusy = false;
                updateAnalysisControls();
                if (r.ok)
                    LogPanel::info(tr("数据已导出：%1").arg(path));
                else if (r.cancelled)
                    LogPanel::info(tr("导出已取消，原始会话仍然保留。"));
                else
                {
                    LogPanel::error(tr("导出失败：%1").arg(r.error));
                    QMessageBox::warning(this, tr("导出失败"), r.error);
                }
                emit exportFinished(r.ok, path, r.cancelled ? tr("已取消") : r.error);
            });
    m_exportWatcher.setFuture(
        QtConcurrent::run([source, path, state] { return exportSession(source, path, state); }));
    return true;
}
void WavePanel::cancelExport()
{
    if (m_exportState)
        m_exportState->cancelled.store(true);
}
void WavePanel::exportDataDialog()
{
    chooseExportPath(std::nullopt);
}

void WavePanel::chooseExportPath(const std::optional<std::array<double, 2>> &range)
{
    if (!sampleCount())
    {
        QMessageBox::information(this, tr("导出数据"), tr("当前没有已记录数据。"));
        return;
    }
    QSettings settings;
    QString filter = tr("CSV 数据 (*.csv)");
    const auto dir = settings.value("export/directory", m_recordingDirectory).toString();
    const auto source = bufferFileName();
    QString name = QFileInfo(source).completeBaseName();
    if (range) name += QString("_range_%1_%2").arg((*range)[0], 0, 'f', 3).arg((*range)[1], 0, 'f', 3);
    QString path = QFileDialog::getSaveFileName(
        this, range ? tr("导出 A/B 区间") : tr("导出当前会话"),
        QDir(dir).filePath(name + ".csv"),
        filter + QStringLiteral(";;") + tr("JSON 数据 (*.json)"), &filter);
    if (path.isEmpty())
        return;
    const auto suffix = QFileInfo(path).suffix().toLower();
    if (suffix != "csv" && suffix != "json")
        path += filter.contains("json") ? ".json" : ".csv";
    if (bufferFileName() != source)
    {
        LogPanel::warning(tr("选择文件期间会话已切换，请重新导出。"));
        return;
    }
    settings.setValue("export/directory", QFileInfo(path).absolutePath());
    startExport(path, range);
}

void WavePanel::saveSessionAsDialog()
{
    if (bufferFileName().isEmpty())
    {
        QMessageBox::information(this, tr("另存会话"), tr("请先创建采集会话。"));
        return;
    }
    QString path = QFileDialog::getSaveFileName(
        this, tr("会话另存为"),
        QDir(m_recordingDirectory)
            .filePath(QFileInfo(bufferFileName()).completeBaseName() + "_copy.swave"),
        tr("采集会话 (*.swave)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".swave", Qt::CaseInsensitive))
        path += ".swave";
    if (!saveSessionAs(path))
        QMessageBox::warning(this, tr("另存失败"),
            m_eventLoadError ? tr("配套事件文件读取失败，无法完整另存。请先检查原文件。") :
            !m_events.errorString().isEmpty() ? m_events.errorString() : m_archive.errorString());
}

bool WavePanel::saveSessionAs(const QString &path)
{
    if (m_eventLoadError || !m_archive.saveAs(path))
        return false;
    if (!m_events.relocate(path))
    {
        reportStorageError(tr("波形已另存，但事件保存失败：%1").arg(m_events.errorString()));
        emit sessionChanged();
        return false;
    }
    m_storageError = false;
    if (m_measureARequest) requestMeasurementRead();
    if (m_viewCancel)
        m_viewCancel->store(true);
    ++m_viewGeneration;
    if (m_paused)
        requestView();
    updateControls();
    emit sessionChanged();
    LogPanel::info(tr("会话与事件已另存，后续记录使用新文件：%1").arg(path));
    return true;
}

void WavePanel::exportPngDialog()
{
    QSettings settings;
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出三图 PNG"),
        settings.value("export/directory", m_recordingDirectory).toString(),
        tr("PNG 图片 (*.png)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(".png", Qt::CaseInsensitive))
        path += ".png";
    settings.setValue("export/directory", QFileInfo(path).absolutePath());
    QPixmap image(1600, 1500);
    image.fill(THEME_BG);
    QPainter painter(&image);
    for (int p = 0; p < waveformPlotCount; ++p)
        painter.drawPixmap(0, p * 500, m_plots[p]->toPixmap(1600, 500));
    painter.end();
    if (!image.save(path))
        QMessageBox::warning(this, tr("导出失败"), tr("无法保存图片：%1").arg(path));
    else
        LogPanel::info(tr("波形图片已导出：%1").arg(path));
}

void WavePanel::setChannelVisible(int channel, bool visible)
{
    if (channel < 0 || channel >= waveformChannelCount)
        return;
    auto *plot = m_plots[channel / 2];
    auto *graph = plot->graph(channel % 2);
    graph->setVisible(visible);
    plot->legend->itemWithPlottable(graph)->setTextColor(visible ? QColor(THEME_TEXT) : QColor(100, 100, 100));
    QSettings().setValue(QString("wave/channel%1Visible").arg(channel), visible);
    m_dirty = true;
}

void WavePanel::setChannelColor(int channel, const QColor &color)
{
    if (channel < 0 || channel >= waveformChannelCount || !color.isValid())
        return;
    auto *graph = m_plots[channel / 2]->graph(channel % 2);
    auto pen = graph->pen();
    pen.setColor(color);
    graph->setPen(pen);
    QSettings().setValue(QString("wave/channel%1Color").arg(channel), color);
    m_dirty = true;
}

bool WavePanel::setFixedYRange(int plot, double lower, double upper)
{
    if (plot < 0 || plot >= waveformPlotCount || !qIsFinite(lower) || !qIsFinite(upper) ||
        upper <= lower || !QCPRange::validRange(lower, upper))
        return false;
    m_autoYBtn->setChecked(false);
    m_plots[plot]->yAxis->setRange(lower, upper);
    m_dirty = true;
    return true;
}

void WavePanel::configureChannels()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("通道与坐标"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *hint = new QLabel(tr("隐藏通道只影响显示，全部通道仍正常采集和保存。"), &dialog);
    layout->addWidget(hint);
    auto *grid = new QGridLayout;
    std::array<QCheckBox *, waveformChannelCount> visible{};
    std::array<QPushButton *, waveformChannelCount> colorButtons{};
    std::array<QColor, waveformChannelCount> selectedColors{};
    auto refreshColor = [&](int c)
    {
        colorButtons[c]->setText(selectedColors[c].name());
        colorButtons[c]->setStyleSheet(QString("color: %1").arg(selectedColors[c].name()));
    };
    for (int c = 0; c < waveformChannelCount; ++c)
    {
        visible[c] = new QCheckBox(tr(names[c]), &dialog);
        visible[c]->setObjectName(QString("channel%1VisibleCheck").arg(c));
        visible[c]->setChecked(m_plots[c / 2]->graph(c % 2)->visible());
        selectedColors[c] = m_plots[c / 2]->graph(c % 2)->pen().color();
        colorButtons[c] = new QPushButton(&dialog);
        refreshColor(c);
        connect(colorButtons[c], &QPushButton::clicked, &dialog, [&, c]
        {
            const QColor color = QColorDialog::getColor(selectedColors[c], &dialog, tr("曲线颜色"));
            if (color.isValid())
            {
                selectedColors[c] = color;
                refreshColor(c);
            }
        });
        grid->addWidget(visible[c], c, 0);
        grid->addWidget(colorButtons[c], c, 1);
    }
    layout->addLayout(grid);
    auto *automatic = new QCheckBox(tr("Y 轴自适应（推杆固定 0～100%）"), &dialog);
    automatic->setObjectName("autoYAxisCheck");
    automatic->setChecked(m_autoY);
    layout->addWidget(automatic);
    auto *ranges = new QGridLayout;
    ranges->addWidget(new QLabel(tr("坐标轴"), &dialog), 0, 0);
    ranges->addWidget(new QLabel(tr("最小值"), &dialog), 0, 1);
    ranges->addWidget(new QLabel(tr("最大值"), &dialog), 0, 2);
    std::array<std::array<QDoubleSpinBox *, 2>, waveformPlotCount> limits{};
    for (int p = 0; p < waveformPlotCount; ++p)
    {
        ranges->addWidget(new QLabel(tr(axisNames[p]), &dialog), p + 1, 0);
        for (int end = 0; end < 2; ++end)
        {
            auto *spin = new QDoubleSpinBox(&dialog);
            limits[p][end] = spin;
            spin->setObjectName(QString("plot%1Limit%2").arg(p).arg(end));
            spin->setRange(-1e12, 1e12);
            spin->setDecimals(4);
            spin->setValue(m_fixedY[p][end]);
            spin->setEnabled(!m_autoY);
            connect(automatic, &QCheckBox::toggled, spin, [spin](bool on) { spin->setEnabled(!on); });
            ranges->addWidget(spin, p + 1, end + 1);
        }
    }
    layout->addLayout(ranges);
    auto *error = new QLabel(&dialog);
    error->setStyleSheet("color: #ef5350");
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                                        QDialogButtonBox::RestoreDefaults, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("取消"));
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(tr("恢复默认"));
    layout->addWidget(buttons);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog, [&]
    {
        automatic->setChecked(true);
        for (int c = 0; c < waveformChannelCount; ++c)
        {
            visible[c]->setChecked(true);
            selectedColors[c] = colors[c];
            refreshColor(c);
        }
        for (int p = 0; p < waveformPlotCount; ++p)
        {
            limits[p][0]->setValue(0);
            limits[p][1]->setValue(defaultYMax[p]);
        }
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]
    {
        for (int p = 0; p < waveformPlotCount; ++p)
            if (limits[p][1]->value() <= limits[p][0]->value())
            {
                error->setText(tr("最大值必须大于最小值。"));
                return;
            }
        for (int c = 0; c < waveformChannelCount; ++c)
        {
            setChannelVisible(c, visible[c]->isChecked());
            setChannelColor(c, selectedColors[c]);
        }
        for (int p = 0; p < waveformPlotCount; ++p)
            setFixedYRange(p, limits[p][0]->value(), limits[p][1]->value());
        m_autoYBtn->setChecked(automatic->isChecked());
        dialog.accept();
    });
    dialog.exec();
}

namespace
{
QString eventTypeName(const QString &type)
{
    if (type == "record_start") return QObject::tr("开始记录");
    if (type == "record_stop") return QObject::tr("停止记录");
    if (type == "connect") return QObject::tr("连接");
    if (type == "disconnect") return QObject::tr("断开");
    if (type == "command") return QObject::tr("发送命令");
    if (type == "note") return QObject::tr("备注");
    return type;
}
}

bool WavePanel::addSessionEvent(const QString &type, const QString &message)
{
    if (m_playback || bufferFileName().isEmpty())
        return false;
    const double time = m_eventClock.isValid() ? double(m_eventClock.nsecsElapsed()) / 1e9 : 0;
    const auto before = m_events.entries().size();
    const bool ok = m_events.append(time, type, message.left(2048));
    if (m_events.entries().size() > before)
        appendEventRow(m_events.entries().back());
    m_dirty = true;
    if (!ok)
        reportStorageError(tr("会话事件保存失败：%1").arg(m_events.errorString()));
    updateControls();
    return ok;
}

void WavePanel::appendEventRow(const SessionEvent &event)
{
    auto *time = new QStandardItem(number(event.time, 3));
    time->setData(event.time, Qt::UserRole);
    auto *message = new QStandardItem(QString(event.message).replace('\r', "\\r").replace('\n', "\\n"));
    message->setToolTip(event.message);
    m_eventModel->appendRow({time, new QStandardItem(eventTypeName(event.type)), message});
}

void WavePanel::rebuildEventModel()
{
    m_eventModel->removeRows(0, m_eventModel->rowCount());
    for (const auto &event : m_events.entries())
        appendEventRow(event);
    m_dirty = true;
}

void WavePanel::addNoteDialog()
{
    if (m_playback || bufferFileName().isEmpty() || m_storageError)
        return;
    bool accepted = false;
    const QString note = QInputDialog::getText(this, tr("添加会话备注"),
        tr("备注记录在当前主机时间（最多 2048 字符）："), QLineEdit::Normal, {}, &accepted).trimmed();
    if (accepted && !note.isEmpty())
        addSessionEvent("note", note);
}

void WavePanel::showEvents()
{
    if (!m_eventsDialog)
    {
        m_eventsDialog = new QDialog(this);
        m_eventsDialog->setWindowTitle(tr("会话事件 · 双击跳转"));
        m_eventsDialog->resize(780, 360);
        auto *layout = new QVBoxLayout(m_eventsDialog);
        auto *hint = new QLabel(tr("按发生顺序记录。双击跳转到对应波形；事件只表示上位机操作，不代表设备执行确认。"), m_eventsDialog);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        auto *table = new QTableView(m_eventsDialog);
        table->setObjectName("sessionEventsTable");
        table->setModel(m_eventModel);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setWordWrap(false);
        table->setColumnWidth(0, 100);
        table->setColumnWidth(1, 100);
        table->horizontalHeader()->setStretchLastSection(true);
        connect(table, &QTableView::doubleClicked, this, [this](const QModelIndex &index)
        { focusEvent(index.row()); });
        layout->addWidget(table);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, m_eventsDialog);
        buttons->button(QDialogButtonBox::Close)->setText(tr("关闭"));
        connect(buttons, &QDialogButtonBox::rejected, m_eventsDialog, &QDialog::hide);
        layout->addWidget(buttons);
    }
    m_eventsDialog->show();
    m_eventsDialog->raise();
    m_eventsDialog->activateWindow();
}

void WavePanel::focusEvent(int row)
{
    if (row < 0 || row >= m_events.entries().size())
        return;
    const auto &event = m_events.entries()[row];
    m_pauseBtn->setChecked(true);
    const double left = qMax(0.0, event.time - m_windowSec * .5);
    setRange(left, left + m_windowSec);
    requestView();
    m_cursorInfo->setText(tr("事件 t=%1 s · %2：%3")
        .arg(number(event.time, 3), eventTypeName(event.type), event.message));
    m_dirty = true;
}

void WavePanel::updateEventMarkers()
{
    const auto range = m_plots[0]->xAxis->range();
    QVector<const SessionEvent *> visible;
    for (auto it = m_events.entries().crbegin(); it != m_events.entries().crend() && visible.size() < 32; ++it)
        if (it->time >= range.lower && it->time <= range.upper)
            visible.prepend(&*it);
    double lastLabel = -1e100;
    for (int i = 0; i < 32; ++i)
    {
        const bool show = i < visible.size();
        for (int p = 0; p < waveformPlotCount; ++p)
        {
            auto *line = m_eventLines[p][i];
            line->setVisible(show);
            if (show)
            {
                line->start->setCoords(visible[i]->time, 0);
                line->end->setCoords(visible[i]->time, 1);
            }
        }
        // 事件密集时保留线，减少重叠文字；详情始终保留在事件列表。
        const bool label = show && visible[i]->time - lastLabel >= range.size() * .13;
        m_eventLabels[i]->setVisible(label);
        if (label)
        {
            lastLabel = visible[i]->time;
            m_eventLabels[i]->position->setCoords(visible[i]->time, .02);
            m_eventLabels[i]->setText(eventTypeName(visible[i]->type));
        }
    }
}

QStringList WavePanel::protectedSessionPaths() const
{
    QStringList paths{bufferFileName()};
    if (m_exportBusy) paths << m_exportSourcePath;
    if (m_viewBusy) paths << m_viewSourcePath;
    if (m_measureBusy) paths << m_measureSourcePath;
    return paths;
}

void WavePanel::clearSavedSessions()
{
    const auto plan = planSessionCleanup(m_recordingDirectory, protectedSessionPaths());
    if (!plan.error.isEmpty())
    {
        QMessageBox::warning(this, tr("无法清理会话"), plan.error);
        return;
    }
    if (plan.files.isEmpty())
    {
        QMessageBox::information(this, tr("清理历史会话"),
            tr("没有可清理的历史会话。当前会话、后台任务正在使用的文件、未知格式及其他导出文件会保留。"));
        return;
    }
    QMessageBox confirm(QMessageBox::Warning, tr("确认清理历史会话"),
        tr("将永久删除 %1 个历史会话及其配套事件文件，共 %2 个文件，约 %3。\n\n"
           "记录目录：%4\n\n当前会话和后台任务正在使用的会话会保留，CSV / JSON / PNG 导出文件不会删除。\n"
           "删除后无法在本程序中恢复，是否确认？")
            .arg(plan.sessions).arg(plan.files.size())
            .arg(QLocale().formattedDataSize(plan.bytes), plan.directory),
        QMessageBox::Yes | QMessageBox::Cancel, this);
    confirm.setObjectName("sessionCleanupConfirmation");
    confirm.button(QMessageBox::Yes)->setText(tr("确认删除"));
    confirm.button(QMessageBox::Cancel)->setText(tr("取消"));
    confirm.setDefaultButton(QMessageBox::Cancel);
    confirm.setEscapeButton(QMessageBox::Cancel);
    QStringList details;
    for (const auto &file : plan.files)
        details << QFileInfo(file.path).fileName();
    confirm.setDetailedText(details.join('\n'));
    if (confirm.exec() != QMessageBox::Yes)
        return;
    // 弹窗期间采集、重连和后台任务仍可能变化，执行时重新获取保护列表。
    const auto result = executeSessionCleanup(plan, protectedSessionPaths());
    QString message = tr("已删除 %1 个历史会话，共 %2 个文件。")
        .arg(result.sessionsRemoved).arg(result.filesRemoved);
    if (result.skipped)
        message += tr("\n%1 个文件因被使用或发生变化而跳过。").arg(result.skipped);
    if (!result.errors.isEmpty())
        message += "\n" + result.errors.join('\n');
    LogPanel::info(message);
    if (result.errors.isEmpty())
        QMessageBox::information(this, tr("会话清理完成"), message);
    else
        QMessageBox::warning(this, tr("部分会话未清理"), message);
}
