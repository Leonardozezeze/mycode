#include "wavepanel.h"
#include "qcustomplot.h"
#include "theme.h"
#include "logpanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QMessageBox>
#include <QPen>
#include <QStringList>
#include <QtMath>

#include <utility>

namespace {

// 实时跟随模式下绘图区保留（并显示）的时间窗口
constexpr double kWindowSec = 10.0;

// 波形通道：顺序与界面上的选择按钮、归档里的列顺序一致。
struct ChannelInfo {
    const char *name;        // 界面按钮文字
    const char *axisLabel;   // Y 轴标题
    const char *csvKey;      // CSV/JSON 导出列名
    double      defaultMin;  // 未开 Y 自适应时的默认 Y 范围
    double      defaultMax;
};

constexpr int kChannelCount = 3;

constexpr ChannelInfo kChannels[kChannelCount] = {
    { "破碎率",   "破碎率 (%)",    "crush_rate",    0.0,   100.0 },
    { "含杂率",   "含杂率 (%)",    "impurity_rate", 0.0,    20.0 },
    { "电机转速", "电机转速 (rpm)", "motor_speed",   0.0,  3000.0 },
};

} // namespace

WavePanel::WavePanel(QWidget *parent)
    : QWidget(parent)
    , m_plot(new QCustomPlot(this))
    , m_replotTimer(new QTimer(this))
    , m_archive(kChannelCount)
{
    // 1. 布局：控制条 + 绘图区
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *toolBar = new QHBoxLayout;
    toolBar->setContentsMargins(8, 2, 4, 0);
    toolBar->setSpacing(6);

    auto *title = new QLabel(tr("波形"), this);
    title->setObjectName("panelTitle");
    toolBar->addWidget(title);
    toolBar->addSpacing(8);

    m_pauseBtn = new QPushButton(tr("暂停"), this);
    m_pauseBtn->setCheckable(true);

    auto *clearBtn = new QPushButton(tr("清空"), this);

    m_autoYBtn = new QPushButton(tr("Y 自适应"), this);
    m_autoYBtn->setCheckable(true);

    auto *exportPngBtn = new QPushButton(tr("导出 PNG"), this);

    // 左侧：暂停 / 清空 / Y 自适应（保持原来的位置）
    toolBar->addWidget(m_pauseBtn);
    toolBar->addWidget(clearBtn);
    toolBar->addWidget(m_autoYBtn);

    // 中间：波形选择标签 + 三个互斥通道按钮
    toolBar->addStretch();
    auto *channelLabel = new QLabel(tr("波形选择"), this);
    channelLabel->setObjectName("hintText");
    toolBar->addWidget(channelLabel);

    // 通道选择（互斥）：只决定绘制哪一路，录制时所有通道都会归档
    auto *channelGroup = new QButtonGroup(this);
    for (int c = 0; c < kChannelCount; ++c) {
        auto *btn = new QPushButton(tr(kChannels[c].name), this);
        btn->setCheckable(true);
        btn->setChecked(c == m_selectedChannel);
        channelGroup->addButton(btn, c);
        toolBar->addWidget(btn);
    }
    connect(channelGroup, &QButtonGroup::idClicked,
            this, &WavePanel::setChannel);
    toolBar->addStretch();

    toolBar->addWidget(exportPngBtn);

    layout->addLayout(toolBar);
    layout->addWidget(m_plot, 1);

    connect(m_pauseBtn, &QPushButton::toggled,
            this, &WavePanel::togglePause);
    connect(m_autoYBtn, &QPushButton::toggled,
            this, &WavePanel::toggleAutoY);
    connect(clearBtn, &QPushButton::clicked,
            this, &WavePanel::clearData);
    connect(exportPngBtn, &QPushButton::clicked,
            this, &WavePanel::exportPngDialog);

    // 暂停时用户拖动/缩放，按可视范围从归档里加载历史数据
    connect(m_plot->xAxis,
            qOverload<const QCPRange &>(&QCPAxis::rangeChanged),
            this, [this](const QCPRange &range) {
                onViewRangeChanged(range.lower, range.upper);
            });

    // 2. QCustomPlot 初始化
    // 注意：Qt6 下 setOpenGl(true) 部分环境会黑屏/崩溃，先注释掉，
    // 性能不够再打开。打开时要放在 setInteractions 之前。
    m_plot->setOpenGl(true);
    // QCustomPlot 2.x 没有 setAutoReplot：只要不调用 replot() 就不会重绘，
    // 重绘节奏完全由下面的定时器控制。
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    // ---- 黑底主题：颜色统一在 theme.h 里改 ----
    // 1. 整体背景 + 绘图区背景
    m_plot->setBackground(QBrush(THEME_BG));
    m_plot->axisRect()->setBackground(QBrush(THEME_BG));

    // 2. 四条坐标轴：轴线、刻度、刻度文字、轴标题统一改成浅色
    for (QCPAxis *ax : {m_plot->xAxis, m_plot->yAxis,
                        m_plot->xAxis2, m_plot->yAxis2}) {
        ax->setBasePen(QPen(THEME_AXIS));
        ax->setTickPen(QPen(THEME_AXIS));
        ax->setSubTickPen(QPen(THEME_AXIS));
        ax->setTickLabelColor(THEME_TEXT);
        ax->setLabelColor(THEME_TEXT);

        // 3. 网格线：黑底上别用纯白，调暗 + 点线
        ax->grid()->setPen(QPen(THEME_GRID, 1, Qt::DotLine));
        ax->grid()->setSubGridPen(QPen(THEME_SUB_GRID, 1, Qt::DotLine));
        ax->grid()->setZeroLinePen(QPen(THEME_ZERO_LINE));
    }

    // 3. 一条曲线
    m_plot->addGraph();
    m_plot->graph(0)->setPen(QPen(THEME_CURVE));

    // 4. 坐标轴初始范围
    m_plot->xAxis->setLabel("Time (s)");
    m_plot->yAxis->setLabel(tr(kChannels[m_selectedChannel].axisLabel));
    m_plot->xAxis->setRange(0, kWindowSec);
    m_plot->yAxis->setRange(kChannels[m_selectedChannel].defaultMin,
                            kChannels[m_selectedChannel].defaultMax);

    // 5. 定时重绘：约 33 FPS
    m_replotTimer->setInterval(30);
    connect(m_replotTimer, &QTimer::timeout,
            this, &WavePanel::onReplotTimeout);
    m_replotTimer->start();
}

void WavePanel::addData(double x, const QVector<double> &values)
{
    // 补齐通道数：缺的通道按 0 记，保证归档里每条记录长度一致
    QVector<double> all = values;
    if (all.size() < kChannelCount)
        all.resize(kChannelCount);

    // 先归档全部通道：完整数据不随显示窗口裁剪，导出/回看历史都用得到。
    // 暂停时也照常归档，保证不丢数据。
    m_archive.append(x, all.constData());

    // 暂停时停止绘制，只记录；恢复时再从归档重建最新窗口
    if (m_paused)
        return;

    // 只把当前选中的通道送进绘图控件
    appendToGraph(x, all.at(m_selectedChannel));

    // 滑动窗口：绘图只保留最近 kWindowSec 秒的数据（归档保留全部）
    m_plot->graph(0)->data()->removeBefore(x - kWindowSec);

    followLatest(x);
}

void WavePanel::setChannel(int channel)
{
    if (channel < 0 || channel >= kChannelCount || channel == m_selectedChannel)
        return;

    m_selectedChannel = channel;
    m_plot->yAxis->setLabel(tr(kChannels[channel].axisLabel));

    if (!m_autoY)
        m_plot->yAxis->setRange(kChannels[channel].defaultMin,
                                kChannels[channel].defaultMax);

    if (m_paused) {
        // 暂停状态：保持当前查看的时间范围，用归档里的新通道数据重画
        m_loadingView = true;
        const QCPRange r = m_plot->xAxis->range();
        loadRangeFromArchive(r.lower, r.upper);
        applyAutoY();
        m_dirty = false;
        m_loadingView = false;
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    } else {
        // 实时状态：回到最新窗口，继续跟随
        followLatestFromArchive();
    }
}

void WavePanel::appendToGraph(double x, double y)
{
    // 只往绘图控件里加点，不归档；标脏后由定时器统一重绘
    m_plot->graph(0)->addData(x, y);
    m_dirty = true;
}

void WavePanel::clearData()
{
    const qint64 recorded = m_archive.count();
    if (recorded > 0) {
        const auto answer = QMessageBox::question(
            this, tr("清空数据"),
            tr("将清空已记录的 %1 个采样点，且无法恢复。确定继续？")
                .arg(recorded),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes)
            return;
    }

    m_archive.clear();
    m_plot->graph(0)->data()->clear();
    m_plot->xAxis->setRange(0, kWindowSec);
    if (!m_autoY)
        m_plot->yAxis->setRange(kChannels[m_selectedChannel].defaultMin,
                                kChannels[m_selectedChannel].defaultMax);
    m_dirty = false;
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WavePanel::followLatest(double x)
{
    // 前 kWindowSec 秒固定在 0~kWindowSec，数据超过窗口后视图才开始滚动，
    // 这样起始阶段不会出现 -10 这种负的时间刻度。
    const double left  = qMax(0.0, x - kWindowSec);
    const double right = qMax(kWindowSec, x);
    m_plot->xAxis->setRange(left, right);
}

void WavePanel::togglePause(bool paused)
{
    m_paused = paused;
    m_pauseBtn->setText(paused ? tr("继续") : tr("暂停"));

    // 恢复绘制：回到最新数据继续跟随。暂停期间的数据都还在归档里。
    if (!paused)
        followLatestFromArchive();
}

void WavePanel::toggleAutoY(bool enabled)
{
    m_autoY = enabled;
    if (enabled) {
        applyAutoY();
        m_dirty = false;
        m_plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

double WavePanel::lastTime() const
{
    double x = 0.0;
    return m_archive.lastX(x) ? x : 0.0;
}

qint64 WavePanel::bufferSize() const
{
    return m_archive.bufferSize();
}

bool WavePanel::exportDataTo(const QString &path)
{
    if (m_archive.count() == 0)
        return false;

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("json"))
        return exportJson(path);
    return exportCsv(path);
}

void WavePanel::onReplotTimeout()
{
    // 暂停时不绘制；没有新数据时也不用空转 replot
    if (m_paused || !m_dirty)
        return;

    applyAutoY();

    // 定时器只负责重绘，排队重绘可以进一步节流
    m_dirty = false;
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WavePanel::applyAutoY()
{
    // Y 轴自适应：按当前绘图区里的数据缩放，并留 10% 余量
    if (!m_autoY || m_plot->graph(0)->dataCount() == 0)
        return;

    m_plot->yAxis->rescale(true);
    const QCPRange r = m_plot->yAxis->range();
    const double pad = qMax(r.size() * 0.1, 1e-6);
    m_plot->yAxis->setRange(r.lower - pad, r.upper + pad);
}

void WavePanel::loadRangeFromArchive(double left, double right)
{
    if (left > right)
        std::swap(left, right);

    m_plot->graph(0)->data()->clear();
    const bool ok = m_archive.forEachSampleInRange(
        left, right,
        [this](double x, const double *values) {
            appendToGraph(x, values[m_selectedChannel]);
            return true;
        });

    if (!ok) {
        LogPanel::warning(tr("读取历史数据失败：%1")
                              .arg(m_archive.errorString()));
    }
}

void WavePanel::followLatestFromArchive()
{
    m_loadingView = true;

    double lastX = 0.0;
    if (m_archive.lastX(lastX)) {
        const double left  = qMax(0.0, lastX - kWindowSec);
        const double right = qMax(kWindowSec, lastX);
        loadRangeFromArchive(left, right);
        m_plot->xAxis->setRange(left, right);
    } else {
        m_plot->graph(0)->data()->clear();
        m_plot->xAxis->setRange(0, kWindowSec);
        if (!m_autoY)
            m_plot->yAxis->setRange(kChannels[m_selectedChannel].defaultMin,
                                    kChannels[m_selectedChannel].defaultMax);
    }

    applyAutoY();
    m_dirty = false;
    m_loadingView = false;
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WavePanel::onViewRangeChanged(double lower, double upper)
{
    // 只在暂停时接管：用户拖动/缩放查看历史，按可视范围从归档重建曲线。
    // 实时跟随模式下由 addData/定时器控制视图，这里不干预。
    if (!m_paused || m_loadingView)
        return;

    m_loadingView = true;
    const double margin = (upper - lower) * 0.1;
    loadRangeFromArchive(lower - margin, upper + margin);
    applyAutoY();
    m_dirty = false;
    m_loadingView = false;

    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void WavePanel::exportDataDialog()
{
    const QString csvFilter  = tr("CSV 数据 (*.csv)");
    const QString jsonFilter = tr("JSON 数据 (*.json)");

    QString selectedFilter = csvFilter;
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出数据"), QString(),
        csvFilter + ";;" + jsonFilter,
        &selectedFilter);
    if (path.isEmpty())
        return;

    // 按扩展名判断格式；用户没写扩展名时按所选过滤器补上
    const QString suffix = QFileInfo(path).suffix().toLower();
    bool json = (suffix == QLatin1String("json"));
    if (!json && suffix != QLatin1String("csv")) {
        json = (selectedFilter == jsonFilter);
        path += json ? QStringLiteral(".json") : QStringLiteral(".csv");
    }

    if (m_archive.count() == 0) {
        QMessageBox::information(this, tr("导出数据"),
                                 tr("还没有记录到任何采样点。"));
        return;
    }

    const bool ok = json ? exportJson(path) : exportCsv(path);
    if (!ok) {
        const QString detail = m_archive.errorString();
        QString message = tr("导出失败：%1").arg(path);
        if (!detail.isEmpty())
            message += QLatin1Char('\n') + detail;
        QMessageBox::warning(this, tr("导出失败"), message);
        return;
    }

    LogPanel::info(tr("已导出 %1 个采样点 → %2")
                       .arg(m_archive.count()).arg(path));
}

void WavePanel::exportPngDialog()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("导出 PNG"), QString(), tr("PNG 图片 (*.png)"));
    if (path.isEmpty())
        return;

    if (!path.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
        path += QStringLiteral(".png");

    if (!exportPng(path)) {
        QMessageBox::warning(this, tr("导出失败"),
                             tr("无法保存图片：%1").arg(path));
        return;
    }

    LogPanel::info(tr("已导出图片 → %1").arg(path));
}

bool WavePanel::exportCsv(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream out(&file);

    QStringList header;
    header << QStringLiteral("time_s");
    for (int c = 0; c < kChannelCount; ++c)
        header << QString::fromLatin1(kChannels[c].csvKey);
    out << header.join(QLatin1Char(',')) << '\n';

    const bool readOk = m_archive.forEachSample(
        [&out](double x, const double *values) {
            out << QString::number(x, 'g', 12);
            for (int c = 0; c < kChannelCount; ++c)
                out << ',' << QString::number(values[c], 'g', 12);
            out << '\n';
            return true;
        });

    out.flush();
    return readOk && out.status() == QTextStream::Ok;
}

bool WavePanel::exportJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream out(&file);
    out << "[\n";

    bool first = true;
    const bool readOk = m_archive.forEachSample(
        [&out, &first](double x, const double *values) {
            if (!first)
                out << ",\n";
            first = false;
            out << "  {\"t\": " << QString::number(x, 'g', 12);
            for (int c = 0; c < kChannelCount; ++c) {
                out << ", \"" << QLatin1String(kChannels[c].csvKey) << "\": "
                    << QString::number(values[c], 'g', 12);
            }
            out << '}';
            return true;
        });

    out << "\n]\n";
    out.flush();
    return readOk && out.status() == QTextStream::Ok;
}

bool WavePanel::exportPng(const QString &path)
{
    return m_plot->savePng(path, 1600, 900);
}
