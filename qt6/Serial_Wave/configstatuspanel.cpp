#include "configstatuspanel.h"
#include "canpanel.h"
#include "serialpanel.h"
#include "statuspanel.h"
#include "recordingpanel.h"
#include <QLabel>
#include <QBoxLayout>
#include <QSettings>
#include <QStackedWidget>
#include <QTabWidget>

ConfigStatusPanel::ConfigStatusPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);   // 布局直接设在 this 上
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(createConfigWidget(), 3);            // 通信配置
    m_recordingPanel = new RecordingPanel(this);
    layout->addWidget(m_recordingPanel, 2);                // 采集记录单独一列
    layout->addWidget(createStatusWidget(), 5);            // 状态区
}
QWidget* ConfigStatusPanel::createConfigWidget()
{
    auto *config = new QTabWidget(this);
    config->setObjectName("communicationConfigTabs");
    config->setDocumentMode(true);
    config->addTab(createSensorConfig(), tr("传感数据接收"));

    auto *console = new QTabWidget(config);
    console->setObjectName("consoleConfigTabs");
    console->setDocumentMode(true);
    m_consoleSerialPanel = new SerialPanel(console, {}, QStringLiteral("console/uart"));
    m_consoleSerialPanel->setObjectName("consoleUartPanel");
    const int uartIndex = console->addTab(m_consoleSerialPanel, tr("UART"));
    connect(m_consoleSerialPanel, &SerialPanel::connectionChanged, console,
            [console, uartIndex](bool connected)
            { console->setTabText(uartIndex, connected ? tr("UART · 已连接") : tr("UART")); });
#ifdef Q_OS_LINUX
    m_consoleCanPanel = new CanPanel(console, CanPanel::Role::Console);
    m_consoleCanPanel->setObjectName("consoleCanPanel");
    const int canIndex = console->addTab(m_consoleCanPanel, tr("CAN"));
    connect(m_consoleCanPanel, &CanPanel::connectionChanged, console,
            [console, canIndex](bool connected)
            { console->setTabText(canIndex, connected ? tr("CAN · 已连接") : tr("CAN")); });
    console->setCurrentIndex(canIndex);
#endif
    config->addTab(console, tr("控制台配置"));
    return config;
}

QWidget* ConfigStatusPanel::createSensorConfig()
{
    auto *configWidget = new QWidget;
    configWidget->setObjectName("configWidget");
    auto *configInner = new QVBoxLayout(configWidget);
    configInner->setContentsMargins(8, 0, 8, 0);
    auto *selector = new QComboBox(configWidget);
    selector->setObjectName("transportSelector");
    selector->setAccessibleName(tr("传感数据接收方式"));
    auto *stack = new QStackedWidget(configWidget);
    m_canPanel = new CanPanel(stack);
    m_canPanel->setObjectName("sensorCanPanel");
    m_serialPanel = new SerialPanel(stack);
    m_serialPanel->setObjectName("sensorUartPanel");
#ifdef Q_OS_LINUX
    selector->addItem(tr("CAN 总线"), QStringLiteral("can"));
#endif
    selector->addItem(tr("串口"), QStringLiteral("serial"));
    selector->setVisible(selector->count() > 1);
    configInner->addWidget(selector);
    configInner->addWidget(stack, 1);
    stack->addWidget(m_canPanel);
    stack->addWidget(m_serialPanel);

    const auto selectTransport = [this, selector, stack]
    {
        const bool canSelected = selector->currentData().toString() == QStringLiteral("can");
        if (canSelected && m_serialPanel->isWaitingForReconnect())
            m_serialPanel->cancelReconnect();
        stack->setCurrentWidget(canSelected ? static_cast<QWidget *>(m_canPanel) : m_serialPanel);
    };
    const int saved = selector->findData(QSettings().value("connection/transport", "can"));
    selector->setCurrentIndex(qMax(0, saved));
    selectTransport();
    connect(selector, &QComboBox::currentIndexChanged, this, [selector, selectTransport]
    {
        QSettings().setValue("connection/transport", selector->currentData());
        selectTransport();
    });
    const auto setConnectionState = [selector](bool connected) { selector->setEnabled(!connected); };
    connect(m_canPanel, &CanPanel::connectionChanged, selector, setConnectionState);
    connect(m_serialPanel, &SerialPanel::connectionChanged, selector, setConnectionState);
    return configWidget;
}
SerialPanel* ConfigStatusPanel::serialPanel() const
{
    return m_serialPanel;
}
CanPanel* ConfigStatusPanel::canPanel() const
{
    return m_canPanel;
}
SerialPanel* ConfigStatusPanel::consoleSerialPanel() const
{
    return m_consoleSerialPanel;
}
CanPanel* ConfigStatusPanel::consoleCanPanel() const
{
    return m_consoleCanPanel;
}
StatusPanel* ConfigStatusPanel::statusPanel() const
{
    return m_statusPanel;
}
RecordingPanel* ConfigStatusPanel::recordingPanel() const
{
    return m_recordingPanel;
}
QWidget* ConfigStatusPanel::createStatusWidget()
{
    auto *statusWidget=new QWidget;
    statusWidget->setObjectName("statusWidget");
    auto *statusInner = new QVBoxLayout(statusWidget);
    statusInner->setContentsMargins(0, 0, 0, 0);   // 边距由 StatusPanel 自己控制
    m_statusPanel = new StatusPanel(statusWidget);
    statusInner->addWidget(m_statusPanel);
    return statusWidget;
}
