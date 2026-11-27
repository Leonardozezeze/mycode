#include "configstatuspanel.h"
#include "canpanel.h"
#include "serialpanel.h"
#include "statuspanel.h"
#include "recordingpanel.h"
#include <QFrame>
#include <QLabel>
#include <QBoxLayout>
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
    m_canPanel = new CanPanel(configWidget);
    m_canPanel->setObjectName("sensorCanPanel");
    configInner->addWidget(m_canPanel, 1);
    return configWidget;
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
