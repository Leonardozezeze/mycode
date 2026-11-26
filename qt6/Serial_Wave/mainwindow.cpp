#include "mainwindow.h"
#include "canframe.h"
#include "canpanel.h"
#include "configstatuspanel.h"
#include "wavepanel.h"
#include "logpanel.h"
#include "uartterminal.h"
#include "serialpanel.h"
#include "commandbar.h"
#include "recordingpanel.h"
#include "protocolparser.h"
#include "statuspanel.h"
#include <QBoxLayout>
#include <QDockWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QSettings>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileInfo>
#include <QTimer>
#include <QSplitter>
#include <QScreen>
#include <QAction>
#include <QToolBar>
#include <QLineEdit>
#include <QShowEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QStackedWidget>
#include <QTabBar>
#include <utility>

namespace
{
constexpr int kLayoutVersion = 4;
constexpr int kConsoleWidth = 520;
constexpr int kWindowHeightRevision = 1;

QSize defaultWindowSize(QScreen *screen)
{
    QSize size(1700, 1300);
    if (screen)
    {
        const auto available = screen->availableGeometry();
        size.setWidth(qMin(size.width(), available.width()));
        size.setHeight(qMin(size.height(), qMax(1, available.height() - 40)));
    }
    return size;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_parser(std::make_unique<ProtocolParser>())
{
    setWindowTitle(tr("Serial Wave · 设备采集与回看"));
    resize(defaultWindowSize(QGuiApplication::primaryScreen()));
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    auto *config = new ConfigStatusPanel(central);
    auto *sensorUart = config->serialPanel();
    auto *sensorCan = config->canPanel();
    auto *consoleUart = config->consoleSerialPanel();
    auto *consoleCan = config->consoleCanPanel();
    m_statusPanel = config->statusPanel();
    m_recordingPanel = config->recordingPanel();
    m_wavePanel = new WavePanel(central);
    m_recordingPanel->setWavePanel(m_wavePanel);
    layout->addWidget(config);
    layout->addWidget(m_wavePanel, 1);
    setCentralWidget(central);
    m_logDock = new QDockWidget(tr("控制台与日志"), this);
    m_logDock->setObjectName("consoleDock");
    m_logDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    auto *console = new QWidget(m_logDock);
    auto *consoleLayout = new QVBoxLayout(console);
    consoleLayout->setContentsMargins(10, 8, 10, 10);
    consoleLayout->setSpacing(8);
    auto *tabs = new QTabBar(console);
    tabs->setObjectName("consoleTabs");
    tabs->setDocumentMode(true);
    tabs->setDrawBase(false);
    tabs->setExpanding(true);
    tabs->setUsesScrollButtons(false);
    tabs->setAccessibleName(tr("控制台模式"));
    const int uartTab = tabs->addTab(tr("UART"));
    const int terminalTab = tabs->addTab(tr("UART 终端"));
    const int canTab = consoleCan ? tabs->addTab(tr("CAN")) : -1;
    consoleLayout->addWidget(tabs);

    auto *consolePages = new QStackedWidget(console);
    auto *commandPage = new QWidget(consolePages);
    commandPage->setObjectName("consoleCommandPage");
    auto *commandLayout = new QVBoxLayout(commandPage);
    commandLayout->setContentsMargins(0, 0, 0, 0);
    commandLayout->setSpacing(10);
    auto *editors = new QStackedWidget(commandPage);
    m_commandBar = new CommandBar(editors);
    editors->addWidget(m_commandBar);
    connect(consoleUart, &SerialPanel::connectionChanged, tabs, [this, tabs, uartTab, terminalTab](bool connected)
    {
        m_commandBar->setConnected(connected);
        tabs->setTabText(uartTab, connected ? tr("UART · 在线") : tr("UART"));
        tabs->setTabToolTip(terminalTab, connected ? tr("UART 已连接") : tr("请先连接控制台 UART"));
    });

    if (consoleCan)
    {
        auto *canCommands = new CanCommandBar(editors);
        editors->addWidget(canCommands);
        connect(consoleCan, &CanPanel::connectionChanged, tabs,
                [canCommands, tabs, canTab](bool connected)
                {
                    canCommands->setConnected(connected);
                    tabs->setTabText(canTab, connected ? tr("CAN · 在线") : tr("CAN"));
                });
        connect(canCommands, &CanCommandBar::commandSubmitted, this,
                [this, consoleCan, canCommands](quint32 id, const QByteArray &payload)
                {
                    const auto idText = formatStandardCanId(id);
                    const auto error = consoleCan->sendCommand(id, payload);
                    if (!error.isEmpty())
                    {
                        LogPanel::warning(tr("CAN %1 发送失败：%2").arg(idText, error));
                        return;
                    }

                    canCommands->rememberQueuedSendId(id);
                    const auto message = tr("已排队 [CAN] %1 · DLC %2 · %3")
                                             .arg(idText)
                                             .arg(payload.size())
                                             .arg(QString::fromLatin1(payload.toHex(' ').toUpper()));
                    logConsoleTransmission(message);
                });
    }

    m_logPanel = new LogPanel(commandPage);
    LogPanel::setInstance(m_logPanel);
    commandLayout->addWidget(editors);
    commandLayout->addWidget(m_logPanel, 1);
    consolePages->addWidget(commandPage);
    auto *terminal = new UartTerminal(consolePages);
    consolePages->addWidget(terminal);
    connect(tabs, &QTabBar::currentChanged, consolePages, [=](int tab)
    {
        if (tab == terminalTab)
        {
            consolePages->setCurrentWidget(terminal);
            terminal->focusTerminal();
        }
        else
        {
            editors->setCurrentIndex(tab == canTab ? 1 : 0);
            consolePages->setCurrentWidget(commandPage);
        }
    });
    tabs->setCurrentIndex(consoleCan ? canTab : uartTab);

    connect(consoleUart, &SerialPanel::dataReceived, terminal, &UartTerminal::receive);
    connect(consoleUart, &SerialPanel::connectionChanged, terminal, &UartTerminal::setConnected);
    connect(terminal, &UartTerminal::inputReady, terminal, [terminal, consoleUart](const QByteArray &bytes)
    {
        const auto written = consoleUart->sendData(bytes);
        if (written != bytes.size())
            terminal->showWriteError(tr("UART 发送未完整排队：%1/%2 字节")
                                         .arg(qMax(qint64(0), written)).arg(bytes.size()));
    });
    consoleLayout->addWidget(consolePages, 1);
    m_logDock->setWidget(console);
    m_logDock->setMinimumWidth(380);
    addDockWidget(Qt::RightDockWidgetArea, m_logDock);
    sizeConsoleDock();
    auto *file = menuBar()->addMenu(tr("文件"));
    auto *newSession = file->addAction(tr("新建采集会话"), m_wavePanel, &WavePanel::clearData);
    newSession->setShortcut(QKeySequence::New);
    auto *open = file->addAction(tr("打开会话…"), m_wavePanel, &WavePanel::openSessionDialog);
    open->setShortcut(QKeySequence::Open);
    file->addAction(tr("导出当前会话…"), m_wavePanel, &WavePanel::exportDataDialog);
    file->addAction(tr("会话另存为…"), m_wavePanel, &WavePanel::saveSessionAsDialog);
    file->addSeparator();
    file->addAction(tr("选择记录保存目录…"), m_wavePanel, &WavePanel::chooseRecordingDirectory);
    file->addAction(tr("打开记录目录"), m_wavePanel, &WavePanel::showRecordingDirectory);
    file->addAction(tr("清理历史会话…"), m_wavePanel, &WavePanel::clearSavedSessions);
    file->addSeparator();
    file->addAction(tr("退出"), this, &QWidget::close);
    auto *view = menuBar()->addMenu(tr("视图"));
    // 入口属于主窗口，控制台关闭后菜单、工具栏和快捷键仍可使用。
    m_consoleAction = new QAction(tr("控制台"), this);
    m_consoleAction->setObjectName("showConsoleAction");
    m_consoleAction->setCheckable(true);
    m_consoleAction->setChecked(true);
    m_consoleAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
    m_consoleAction->setToolTip(tr("显示或隐藏控制台（Ctrl+L）"));
    addAction(m_consoleAction);
    view->addAction(m_consoleAction);
    connect(m_consoleAction, &QAction::triggered, this,
            [this](bool visible)
            {
                m_logDock->setVisible(visible);
                if (visible)
                    m_logDock->raise();
            });
    connect(m_logDock, &QDockWidget::visibilityChanged, this,
            [this](bool visible) { m_consoleAction->setChecked(visible); });
    auto *reset = view->addAction(tr("恢复默认布局"), this, &MainWindow::resetLayout);
    reset->setObjectName("resetLayoutAction");
    m_viewToolbar = addToolBar(tr("视图"));
    m_viewToolbar->setObjectName("viewToolbar");
    m_viewToolbar->setMovable(false);
    m_viewToolbar->setFloatable(false);
    m_viewToolbar->addAction(m_consoleAction);
    m_viewToolbar->addAction(reset);
    auto *searchLog = view->addAction(tr("搜索日志"), this,
                                      [this, tabs, terminalTab, editors, uartTab, canTab]
                                      {
                                          m_logDock->show();
                                          m_logDock->raise();
                                          if (tabs->currentIndex() == terminalTab)
                                              tabs->setCurrentIndex(editors->currentIndex() == 0 ? uartTab : canTab);
                                          auto *search =
                                              m_logPanel->findChild<QLineEdit *>("logSearch");
                                          search->setFocus();
                                          search->selectAll();
                                      });
    searchLog->setObjectName("searchLogAction");
    searchLog->setShortcut(QKeySequence::Find);
    auto *help = menuBar()->addMenu(tr("帮助"));
    auto *usage = help->addAction(
        tr("操作说明与快捷键"), this,
        [this]
        {
            QDialog dialog(this);
            dialog.setWindowTitle(tr("操作说明与快捷键"));
            dialog.resize(600, 500);
            auto *layout = new QVBoxLayout(&dialog);
            auto *text = new QTextBrowser(&dialog);
            text->setHtml(
                tr("<h3>控制台与调试</h3>"
                   "<p><b>Ctrl+L</b> 显示/隐藏控制台；<b>Ctrl+F</b> 搜索日志；<b>F1</b> "
                   "打开本说明。</p>"
                   "<p>在控制台配置里可分别连接 UART 和 CAN，两者可同时在线。"
                   "控制台的连接独立于传感数据接收。"
                   "CAN 发送 ID 按目标设备填写十六进制 000～7FF，默认 650。"
                   "首字节是命令号，后面是 0～7 字节 HEX 参数。"
                   "日志中的已排队表示系统接受了发送请求。</p>"
                   "<p>控制台 CAN 回复 ID 使用 11 位标准帧，以十六进制填写 000～7FF。"
                   "可以用逗号或空格填写多项，留空接收所有标准数据帧。</p>"
                   "<p>串口发送区支持 UTF-8 文本和 HEX。HEX 示例：<code>AA 55 01</code>，不加 "
                   "0x，也不追加换行。"
                   "收藏和历史只填入编辑框，仍需回车或点击发送。未连接时也可编辑和收藏。</p>"
                   "<p>右侧控制台把 UART、UART 终端和 CAN 放在同一排页签。"
                   "连接控制台 UART 后可打开 UART 终端，点击终端直接输入。"
                   "回车和退格的发送格式可选。Ctrl+C 会中断下位机 Shell，"
                   "Ctrl+Shift+C 复制，Ctrl+Shift+V 粘贴。</p>"
                   "<p>日志可按关键字、级别和 RX/TX/系统来源筛选。冻结日志只冻结视图，采集继续；"
                   "缓冲保留最近 2000 条。保存菜单可选择当前筛选或全部缓冲。</p>"
                   "<p>接收显示可选文本、原始 HEX 或关闭。HEX 显示有速率限制，超过时提示省略字节；"
                   "波形解析和会话记录仍完整进行。</p>"
                   "<h3>波形与会话</h3>"
                   "<p>Linux 默认使用 CAN。配置并启用系统的 SocketCAN 接口后，填写接口名及标准帧 ID"
                   "（默认 0x123），再连接。每帧 8 字节，含杂率在前、破碎率在后，均为小端 float32 原始比例。</p>"
                   "<p>串口列表每 1.5 秒自动刷新。可勾选异常断开后自动重连；手动断开不会重连，"
                   "等待时点击取消重连可停止。优先匹配唯一序列号，无序列号仅尝试原端口。</p>"
                   "<p>新增推杆行程图（0～100%）及行程/状态卡片，支持伸出、缩回、停止、端点和故障状态。"
                   "旧数据没有推杆反馈时显示暂无数据；上报格式请查看 README。</p>"
                   "<p>通道与坐标可设置五条曲线显示、颜色和三幅图的固定 Y 轴范围；只影响显示。"
                   "添加备注记录当前时间；事件列表可双击跳转。开始/停止、连接/断开和发送命令自动记录。</p>"
                   "<p>事件保存在同名 .swave.events.jsonl 文件中，移动会话时请把两个文件一起带走；"
                   "会话另存为会复制两者。事件不包含设备执行确认。CSV/JSON 导出仍只包含采样数据。</p>"
                   "<p>拖动或滚轮缩放时间轴会冻结波形视图；回到最新恢复跟随。"
                   "Ctrl+滚轮缩放 Y，Shift+拖动平移 Y。区间统计使用全部原始已记录数据。</p>"
                   "<p>查看全程可显示已记录数据的完整时间范围；定位时间可按会话秒数跳转，三幅图同步。</p>"
                   "<p>开启 A/B 测量后依次单击波形选择两个点，自动吸附到最近的原始已记录采样。"
                   "第三次单击重新选择 A。测量详情显示五路 A/B、差值及区间全部原始数据的统计；"
                   "导出 A/B 仅导出闭区间内已记录数据，保留原时间。缺失反馈显示 --，不插值。</p>"
                   "<p>测量期间采集与保存继续，关闭 A/B 测量恢复拖动；恢复跟随、新建或打开会话会清除测量。</p>"
                   "<p>连接传感设备后自动开始记录。停止记录后仍可实时监测；新建会话保留旧文件并重置统计。"
                   "断开传感连接后可打开 .swave 历史会话回看。文件菜单可更换目录、另存和导出。</p>"
                   "<p>采集会话中的清理历史会话按钮会先显示数量、大小和文件明细，默认取消。"
                   "确认后永久删除记录目录里的历史会话及配套事件，当前会话和后台使用中的会话会保留。</p>"
                   "<p>横轴是主机接收时间，不是设备采样时刻；原串口帧继续支持，推杆使用可选扩展帧，均未增加 CRC。</p>"));
            layout->addWidget(text);
            auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
            connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
            layout->addWidget(buttons);
            dialog.exec();
        });
    usage->setShortcut(QKeySequence::HelpContents);
    m_rawReceiveTimer = new QTimer(this);
    m_rawReceiveTimer->setSingleShot(true);
    m_rawReceiveTimer->setInterval(100);
    connect(m_rawReceiveTimer, &QTimer::timeout, this, &MainWindow::flushRawReceive);
    m_textReceiveTimer = new QTimer(this);
    m_textReceiveTimer->setSingleShot(true);
    m_textReceiveTimer->setInterval(200);
    connect(m_textReceiveTimer, &QTimer::timeout, this, &MainWindow::flushDeviceText);
    connect(m_logPanel, &LogPanel::aboutToSave, this,
            [this]
            {
                flushRawReceive();
                flushDeviceText();
            });
    connect(m_logPanel, &LogPanel::receiveModeChanged, this,
            [this]
            {
                m_rxText.clear();
                clearReceiveBatch();
                m_textReceiveTimer->stop();
            });
    connect(m_recordingPanel, &RecordingPanel::exportRequested, m_wavePanel,
            &WavePanel::exportDataDialog);
    connect(sensorUart, &SerialPanel::dataReceived, this,
            [this](const QByteArray &data) { m_parser->feed(data); });
    connect(sensorCan, &CanPanel::frameReceived, this,
            [this](quint32, const QByteArray &payload) { m_parser->feedCan(payload); });
    connect(consoleUart, &SerialPanel::dataReceived, this,
            [this](const QByteArray &data)
            {
                if (m_logPanel->receiveMode() == LogPanel::ReceiveMode::Hex)
                    queueRawReceive(data);
                else if (m_logPanel->receiveMode() == LogPanel::ReceiveMode::Text)
                    appendDeviceText(data);
            });
    if (consoleCan)
        connect(consoleCan, &CanPanel::frameReceived, this, &MainWindow::queueCanReceive);
    m_parser->setFrameHandler(
        [this](const ProtocolFrame &f)
        { m_wavePanel->addSample(f); });
    m_parser->setStatusHandler([this](const MachineStatus &s) { m_statusPanel->updateStatus(s); });
    connect(m_wavePanel, &WavePanel::sessionStarted, this,
            [this]
            {
                m_parser->resetSession(m_wavePanel->sessionClock());
                m_statusPanel->reset();
            });
    connect(m_wavePanel, &WavePanel::sessionChanged, this,
            [this]
            {
                m_statusPanel->setReplayMode(m_wavePanel->isPlayback());
                const auto path = m_wavePanel->bufferFileName();
                if (!path.isEmpty())
                    statusBar()->showMessage(tr("%1：%2").arg(
                        m_wavePanel->isPlayback() ? tr("历史会话") : tr("会话保存在"), path));
            });
    connect(m_wavePanel, &WavePanel::storageFailed, this,
            [this](const QString &message)
            {
                LogPanel::error(message);
                m_logDock->show();
                statusBar()->showMessage(tr("保存失败，采集已停止：%1").arg(message));
                // 用排队调用，避免在解析器的帧回调内启动嵌套事件循环。
                QTimer::singleShot(
                    0, this,
                    [this, message]
                    {
                        QMessageBox::warning(
                            this, tr("会话保存失败"),
                            message + tr("\n已停止追加记录。请检查磁盘空间和目录权限；也可通过“文件"
                                         " → 会话另存为”将内存中的数据一并保存到其他磁盘。"));
                    });
            });
    const auto onSensorConnectionChanged =
        [this, open, sensorUart](bool connected, bool can)
        {
            m_sensorConnected = connected;
            m_statusPanel->setConnected(connected);
            m_wavePanel->setConnected(connected);
            open->setEnabled(!connected);
            const auto transport = can ? tr("CAN") : tr("UART");
            m_parser->resetStream();
            if (connected)
            {
                const auto message = tr("传感 %1 已连接").arg(transport);
                LogPanel::info(message);
                if (!m_wavePanel->isRecording())
                    m_wavePanel->startRecording();
                m_wavePanel->addSessionEvent("connect", message);
            }
            else
            {
                const auto message = !can && sensorUart->isWaitingForReconnect()
                    ? tr("传感 UART 异常断开，等待原设备重连")
                    : tr("传感 %1 已断开").arg(transport);
                LogPanel::info(message);
                m_wavePanel->addSessionEvent("disconnect", message);
                if (m_wavePanel->stopRecording() && m_recordingPanel->autoExport())
                    autoExportRecording();
            }
        };
    connect(sensorUart, &SerialPanel::connectionChanged, this,
            [onSensorConnectionChanged](bool connected) { onSensorConnectionChanged(connected, false); });
    connect(sensorCan, &CanPanel::connectionChanged, this,
            [onSensorConnectionChanged](bool connected) { onSensorConnectionChanged(connected, true); });
    connect(consoleUart, &SerialPanel::connectionChanged, this, [this](bool connected)
    {
        if (!connected)
        {
            flushRawReceive();
            flushDeviceText();
            m_rxText.clear();
        }
        LogPanel::info(connected ? tr("控制台 UART 已连接") : tr("控制台 UART 已断开"));
    });
    if (consoleCan)
        connect(consoleCan, &CanPanel::connectionChanged, this, [this](bool connected)
        {
            if (!connected)
                flushRawReceive();
            LogPanel::info(connected ? tr("控制台 CAN 已连接") : tr("控制台 CAN 已断开"));
        });
    connect(m_commandBar, &CommandBar::commandSubmitted, this,
            [this, consoleUart](const QByteArray &payload)
            {
                const qint64 written = consoleUart->sendData(payload);
                if (written != payload.size())
                {
                    LogPanel::warning(tr("UART 发送未完整排队：%1/%2 字节")
                                          .arg(qMax(qint64(0), written))
                                          .arg(payload.size()));
                    return;
                }
                const auto data = m_commandBar->isHexMode()
                    ? QStringLiteral("[HEX] ") + QString::fromLatin1(payload.toHex(' ').toUpper())
                    : QString::fromUtf8(payload);
                const auto message = tr("已排队 [UART] ") + data;
                m_commandBar->acceptSubmission();
                logConsoleTransmission(message);
            });
    QSettings settings;
    const bool currentLayout = settings.value("window/layoutVersion", 0).toInt() == kLayoutVersion;
    if (currentLayout && settings.contains("window/geometry"))
        restoreGeometry(settings.value("window/geometry").toByteArray());
    // 升级时只增加一次窗口高度，保留用户的控制台位置和分隔比例。
    if (settings.value("window/heightRevision", 0).toInt() < kWindowHeightRevision)
        resize(width(), qMax(height(), defaultWindowSize(screen()).height()));
    if (currentLayout && settings.contains("window/state"))
        m_applyDefaultConsoleWidth =
            !restoreState(settings.value("window/state").toByteArray(), kLayoutVersion);
    if (currentLayout)
        if (auto *splitter = m_wavePanel->findChild<QSplitter *>("waveSplitter"))
            splitter->restoreState(settings.value("window/waveSplitter").toByteArray());
    m_consoleAction->setChecked(!m_logDock->isHidden());
    statusBar()->showMessage(tr("就绪 · 连接传感设备后自动开始记录 · 文件菜单可打开历史会话"));
}
MainWindow::~MainWindow() = default;

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (m_applyDefaultConsoleWidth)
    {
        m_applyDefaultConsoleWidth = false;
        // 窗口首次完成布局后再分配宽度，避免小的初始尺寸把控制台压回最小宽度。
        QTimer::singleShot(0, this, &MainWindow::sizeConsoleDock);
    }
}

void MainWindow::sizeConsoleDock()
{
    const int preferred = qBound(m_logDock->minimumWidth(), qRound(width() * 0.32), kConsoleWidth);
    resizeDocks({m_logDock}, {preferred}, Qt::Horizontal);
}

void MainWindow::resetLayout()
{
    m_logDock->setFloating(false);
    addDockWidget(Qt::RightDockWidgetArea, m_logDock);
    m_logDock->show();
    m_logDock->raise();
    m_viewToolbar->show();
    resize(width(), qMax(height(), defaultWindowSize(screen()).height()));
    sizeConsoleDock();
    m_wavePanel->resetPlotLayout();
}
void MainWindow::appendDeviceText(const QByteArray &bytes)
{
    for (char ch : bytes)
    {
        const auto c = uchar(ch);
        if (ch == '\n')
        {
            if (!m_rxText.trimmed().isEmpty())
                LogPanel::received(QStringLiteral("[UART] ") + QString::fromUtf8(m_rxText).trimmed());
            m_rxText.clear();
        }
        else if (ch == '\r')
        {
            continue;
        }
        else if (c >= 0x20 || ch == '\t')
        {
            if (m_rxText.size() < 4096)
                m_rxText.append(ch);
            else
            {
                m_rxText.clear();
                LogPanel::warning(tr("设备文本过长，已截断。"));
            }
        }
        else
        {
            m_rxText.clear();
        }
    }
    if (!m_rxText.isEmpty())
        m_textReceiveTimer->start();
    else
        m_textReceiveTimer->stop();
}
void MainWindow::flushDeviceText()
{
    m_textReceiveTimer->stop();
    // UTF-8 被串口分包时保留未完成字符，等待后续字节。
    if (m_logPanel->receiveMode() != LogPanel::ReceiveMode::Text || !m_rxText.isValidUtf8())
        return;
    if (!m_rxText.trimmed().isEmpty())
        LogPanel::received(QStringLiteral("[UART] ") + QString::fromUtf8(m_rxText).trimmed());
    m_rxText.clear();
}
void MainWindow::queueRawReceive(const QByteArray &bytes)
{
    // 只限制调试日志的显示量，不影响完整协议解析和会话归档。
    constexpr int maxBatchBytes = 512;
    const auto retained = qMin(qsizetype(maxBatchBytes) - m_rawReceiveBuffer.size(), bytes.size());
    if (retained > 0)
        m_rawReceiveBuffer.append(bytes.constData(), retained);
    m_rawReceiveOmitted += quint64(bytes.size() - qMax(qsizetype(0), retained));
    if (!m_rawReceiveTimer->isActive())
        m_rawReceiveTimer->start();
}

void MainWindow::queueCanReceive(quint32 id, const QByteArray &payload)
{
    if (m_logPanel->receiveMode() == LogPanel::ReceiveMode::Disabled)
        return;
    if (m_canReceiveBatch.size() < 64)
    {
        m_canReceiveBatch.append(tr("[CAN] %1 · DLC %2 %3")
                                     .arg(formatStandardCanId(id)).arg(payload.size())
                                     .arg(QString::fromLatin1(payload.toHex(' ').toUpper())).trimmed());
    }
    else
        ++m_canReceiveOmitted;
    if (!m_rawReceiveTimer->isActive())
        m_rawReceiveTimer->start();
}

void MainWindow::flushRawReceive()
{
    if (m_logPanel->receiveMode() == LogPanel::ReceiveMode::Hex)
    {
        for (qsizetype offset = 0; offset < m_rawReceiveBuffer.size(); offset += 32)
            LogPanel::received(
                QStringLiteral("[UART] [HEX] ") +
                QString::fromLatin1(m_rawReceiveBuffer.mid(offset, 32).toHex(' ').toUpper()));
        if (m_rawReceiveOmitted)
            LogPanel::received(tr("HEX 显示限速：本批另有 %1 字节未展示，解析和采集继续。")
                                   .arg(m_rawReceiveOmitted),
                               LogPanel::Level::Warning);
    }
    for (const auto &message : std::as_const(m_canReceiveBatch))
        LogPanel::received(message);
    if (m_canReceiveOmitted)
        LogPanel::received(tr("CAN 日志限速：本批另有 %1 帧未展示。").arg(m_canReceiveOmitted),
                           LogPanel::Level::Warning);
    clearReceiveBatch();
}

void MainWindow::clearReceiveBatch()
{
    m_rawReceiveTimer->stop();
    m_canReceiveBatch.clear();
    m_canReceiveOmitted = 0;
    m_rawReceiveBuffer.clear();
    m_rawReceiveOmitted = 0;
}

void MainWindow::logConsoleTransmission(const QString &message)
{
    if (m_sensorConnected)
        m_wavePanel->addSessionEvent("command", message);
    LogPanel::transmitted(message);
}

void MainWindow::autoExportRecording()
{
    if (!m_wavePanel->sampleCount() || m_wavePanel->isPlayback())
        return;
    // 会话名包含毫秒和随机后缀；重复导出不会覆盖已有文件。
    const auto path = m_wavePanel->bufferFileName();
    const auto csv = QFileInfo(path).absolutePath() + "/" + QFileInfo(path).completeBaseName() +
                     "_" + QDateTime::currentDateTime().toString("HHmmsszzz") + ".csv";
    m_wavePanel->exportDataTo(csv);
}
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_wavePanel->stopRecording())
    {
        QMessageBox::warning(this, tr("退出前保存失败"),
                             tr("尚有采样数据或会话事件未能写入。请释放磁盘空间后重试，或使用“文件 → "
                                "会话另存为”保存到其他磁盘。\n会话文件：%1")
                                 .arg(m_wavePanel->bufferFileName()));
        event->ignore();
        return;
    }
    m_wavePanel->cancelExport();
    QSettings settings;
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("window/layoutVersion", kLayoutVersion);
    settings.setValue("window/heightRevision", kWindowHeightRevision);
    settings.setValue("window/state", saveState(kLayoutVersion));
    if (auto *splitter = m_wavePanel->findChild<QSplitter *>("waveSplitter"))
        settings.setValue("window/waveSplitter", splitter->saveState());
    event->accept();
}
