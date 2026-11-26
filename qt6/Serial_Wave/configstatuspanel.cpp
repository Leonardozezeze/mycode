#include "configstatuspanel.h"
#include "canpanel.h"
#include "serialpanel.h"
#include "statuspanel.h"
#include "recordingpanel.h"
#include <QFrame>
#include <QLabel>
#include <QBoxLayout>
#include <QSettings>
#include <QStackedWidget>
#include <QTabWidget>

ConfigStatusPanel::ConfigStatusPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(createConfigWidget(), 3);
    m_recordingPanel = new RecordingPanel(this);
    m_recordingPanel->setProperty("workspaceCard", true);
    m_recordingPanel->setAttribute(Qt::WA_StyledBackground);
    layout->addWidget(m_recordingPanel, 2);
    m_statusPanel = new StatusPanel(this);
    m_statusPanel->setProperty("workspaceCard", true);
    m_statusPanel->setAttribute(Qt::WA_StyledBackground);
    layout->addWidget(m_statusPanel, 5);
}
QWidget* ConfigStatusPanel::createConfigWidget()
{
    auto *card = new QFrame(this);
    card->setProperty("workspaceCard", true);
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 8, 8, 3);
    cardLayout->setSpacing(3);
    auto *title = new QLabel(tr("通信配置"), card);
    title->setObjectName("panelTitle");
    cardLayout->addWidget(title);
    auto *config = new QTabWidget(card);
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
    m_consoleCanPanel = new CanPanel(console, CanPanel::Role::Console);
    m_consoleCanPanel->setObjectName("consoleCanPanel");
    const int canIndex = console->addTab(m_consoleCanPanel, tr("CAN"));
    connect(m_consoleCanPanel, &CanPanel::connectionChanged, console,
            [console, canIndex](bool connected)
            { console->setTabText(canIndex, connected ? tr("CAN · 已连接") : tr("CAN")); });
#ifdef Q_OS_LINUX
    console->setCurrentIndex(canIndex);
#endif
    config->addTab(console, tr("控制台配置"));
    cardLayout->addWidget(config);
    return card;
}

QWidget* ConfigStatusPanel::createSensorConfig()
{
    auto *configWidget = new QWidget;
    configWidget->setObjectName("configWidget");
    auto *configInner = new QVBoxLayout(configWidget);
    configInner->setContentsMargins(4, 4, 4, 0);
    configInner->setSpacing(3);
    auto *selector = new QComboBox(configWidget);
    selector->setObjectName("transportSelector");
    selector->setAccessibleName(tr("传感数据接收方式"));
    auto *stack = new QStackedWidget(configWidget);
    m_canPanel = new CanPanel(stack);
    m_canPanel->setObjectName("sensorCanPanel");
    m_serialPanel = new SerialPanel(stack);
    m_serialPanel->setObjectName("sensorUartPanel");
    selector->addItem(tr("CAN 总线"), QStringLiteral("can"));
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
#ifdef Q_OS_LINUX
    const auto defaultTransport = QStringLiteral("can");
#else
    const auto defaultTransport = QStringLiteral("serial");
#endif
    const int saved = selector->findData(QSettings().value("connection/transport", defaultTransport));
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
