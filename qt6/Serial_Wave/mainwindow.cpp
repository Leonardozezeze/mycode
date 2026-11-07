#include "mainwindow.h"
#include "configstatuspanel.h"
#include "wavepanel.h"
#include "logpanel.h"
#include "serialpanel.h"
#include "commandbar.h"
#include "recordingpanel.h"
#include "protocolparser.h"
#include "statuspanel.h"

#include <QBoxLayout>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QStringList>

namespace {

// 临时判断：收到的字节是不是可读 ASCII 文本。
// 等协议解析层做好后，这段分流逻辑应该挪到解析层去。
bool looksLikeText(const QByteArray &data)
{
    if (data.isEmpty())
        return false;
    for (char ch : data) {
        const unsigned char c = static_cast<unsigned char>(ch);
        if (c == '\r' || c == '\n' || c == '\t')
            continue;
        if (c < 0x20 || c >= 0x7f)   // 控制字符或非 ASCII，按二进制处理
            return false;
    }
    return true;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(std::make_unique<ProtocolParser>())
{
    resize(1400, 700);
    setCentralWidget(createCentralWidget());
}

QWidget* MainWindow::createCentralWidget()
{
    auto *m_central = new QWidget(this);
    auto *main_layout = new QHBoxLayout(m_central);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(4);

    main_layout->addWidget(createCSWaveWidget(), 5);
    main_layout->addWidget(createConsoleLogWidget(), 3);

    // ---- 命令条 → 串口 ----
    connect(m_commandBar, &CommandBar::commandSubmitted, this,
            [this](const QByteArray &payload) {
        if (!m_serialPanel || m_serialPanel->sendData(payload) < 0) {
            LogPanel::warning(tr("发送失败：串口未连接"));
            return;
        }
        LogPanel::info(QStringLiteral("[TX] ")
                       + QString::fromUtf8(payload).trimmed());
    });

    // ---- 串口 → 协议解析器；纯文本另外进日志方便调试 ----
    connect(m_serialPanel, &SerialPanel::dataReceived, this,
            [this](const QByteArray &data) {
        if (m_parser)
            m_parser->feed(data);

        if (!looksLikeText(data))
            return;
        QString text = QString::fromUtf8(data);
        text.remove('\r');
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines)
            LogPanel::info(QStringLiteral("[RX] ") + line.trimmed());
    });

    // ---- 连接状态 → 命令条可用性 + 日志 + 可选的自动导出 ----
    connect(m_serialPanel, &SerialPanel::connectionChanged, this,
            [this](bool connected) {
        m_commandBar->setConnected(connected);
        LogPanel::info(connected ? tr("串口已连接") : tr("串口已断开"));

        if (!connected && m_parser)
            m_parser->resetStream();   // 丢掉残留半帧，避免和新连接的数据拼接

        if (!connected && m_recordingPanel
            && m_recordingPanel->autoExport()) {
            autoExportRecording();
        }
    });

    return m_central;
}

QWidget* MainWindow::createConsoleLogWidget()
{
    auto *console = new QWidget(this);
    auto *layout = new QVBoxLayout(console);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_commandBar = new CommandBar(console);
    auto *logPanel = new LogPanel(console);
    LogPanel::setInstance(logPanel);

    layout->addWidget(m_commandBar);   // 命令框在日志栏上面
    layout->addWidget(logPanel, 1);
    return console;
}

QWidget* MainWindow::createCSWaveWidget()
{
    auto *cswave = new QWidget(this);
    auto *layout = new QVBoxLayout(cswave);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    m_wavePanel = new WavePanel(cswave);

    auto *configStatus = new ConfigStatusPanel(cswave);
    m_serialPanel = configStatus->serialPanel();
    m_statusPanel = configStatus->statusPanel();

    // 采集记录面板需要读取波形归档状态，并把“导出…”接到波形导出
    m_recordingPanel = configStatus->recordingPanel();
    m_recordingPanel->setWavePanel(m_wavePanel);
    connect(m_recordingPanel, &RecordingPanel::exportRequested,
            m_wavePanel, &WavePanel::exportDataDialog);

    // 协议里没有时间戳：帧序号按这个标称采样率换算成秒。
    // 设备实际帧率不是 20 Hz 时改这里。
    m_parser->setSampleRate(20.0);
    m_parser->setFrameHandler([this](const ProtocolFrame &frame) {
        m_wavePanel->addData(frame.time,
                             {frame.crushRate, frame.impurityRate, frame.rpm});
    });
    m_parser->setStatusHandler([this](const MachineStatus &status) {
        m_statusPanel->updateStatus(status);
    });

    // 配置区按内容高度，剩余空间全部给波形，避免中间出现大片空白
    layout->addWidget(configStatus, 0);
    layout->addWidget(m_wavePanel, 1);
    return cswave;
}

void MainWindow::autoExportRecording()
{
    if (!m_wavePanel || m_wavePanel->sampleCount() == 0)
        return;

    QString dir = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (dir.isEmpty())
        dir = QDir::homePath();
    dir += QStringLiteral("/Serial_Wave");

    if (!QDir().mkpath(dir)) {
        LogPanel::error(tr("自动导出失败：无法创建目录 %1").arg(dir));
        return;
    }

    const QString name = QStringLiteral("wave_%1.csv")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    const QString path = QDir(dir).filePath(name);

    if (m_wavePanel->exportDataTo(path)) {
        LogPanel::info(tr("已自动导出 %1 个采样点 → %2")
                           .arg(m_wavePanel->sampleCount()).arg(path));
    } else {
        LogPanel::error(tr("自动导出失败：%1").arg(path));
    }
}

MainWindow::~MainWindow() = default;
