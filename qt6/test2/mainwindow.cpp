#include "mainwindow.h"
#include "QLabel"
#include "QBoxLayout"
#include "QPushButton"
#include "QLineEdit"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QWidget* m_central=createCentralWidget();
    setCentralWidget(m_central);
}
QWidget* MainWindow::createCentralWidget() {
    QWidget* m_central = new QWidget;
    QVBoxLayout *main_layout = new QVBoxLayout(m_central);
    main_layout->addWidget(config_statusWidget());
    main_layout->addWidget(WaveWidget());
    return m_central;
}
QWidget* MainWindow::config_statusWidget(){
    QWidget* config_statusWidget=new QWidget;
    QHBoxLayout* config_statusLayout=new QHBoxLayout;
    config_statusWidget->setLayout(config_statusLayout);
    QWidget* configWidget=new QWidget;
    QWidget* statusWidget=new QWidget;
    config_statusLayout->addWidget(configWidget);
    config_statusLayout->addWidget(statusWidget);
    return config_statusWidget;
}
QWidget* MainWindow::WaveWidget(){
    QWidget* WaveWidget=new QWidget;
    return WaveWidget;
}

MainWindow::~MainWindow() = default;
