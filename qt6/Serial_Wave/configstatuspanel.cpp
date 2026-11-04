#include "configstatuspanel.h"
#include <QLabel>
#include <QBoxLayout>
ConfigStatusPanel::ConfigStatusPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);   // 布局直接设在 this 上
    layout->addWidget(createConfigWidget());
    layout->addWidget(createStatusWidget());
}
QWidget* ConfigStatusPanel::createConfigWidget()
{
    auto *configWidget = new QWidget;
    configWidget->setObjectName("configWidget");
    auto *configInner = new QVBoxLayout(configWidget);
    configInner->addWidget(new QLabel("配置区（待填）"));
    return configWidget;
}
QWidget* ConfigStatusPanel::createStatusWidget()
{
    auto *statusWidget=new QWidget;
    statusWidget->setObjectName("statusWidget");
    auto *statusInner = new QVBoxLayout(statusWidget);
    statusInner->addWidget(new QLabel("状态区"));
    return statusWidget;
}