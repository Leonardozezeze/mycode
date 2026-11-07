#include "configstatuspanel.h"
#include "serialpanel.h"
#include "statuspanel.h"
#include "recordingpanel.h"
#include <QLabel>
#include <QBoxLayout>

ConfigStatusPanel::ConfigStatusPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);   // 布局直接设在 this 上
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addWidget(createConfigWidget(), 3);            // 串口配置 + 设备信息
    m_recordingPanel = new RecordingPanel(this);
    layout->addWidget(m_recordingPanel, 2);                // 采集记录单独一列
    layout->addWidget(createStatusWidget(), 5);            // 状态区
}
QWidget* ConfigStatusPanel::createConfigWidget()
{
    auto *configWidget = new QWidget;
    configWidget->setObjectName("configWidget");
    auto *configInner = new QVBoxLayout(configWidget);
    configInner->setContentsMargins(0, 0, 0, 0);   // 边距由 SerialPanel 自己控制
    m_serialPanel = new SerialPanel(configWidget);
    configInner->addWidget(m_serialPanel);
    return configWidget;
}
SerialPanel* ConfigStatusPanel::serialPanel() const
{
    return m_serialPanel;
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