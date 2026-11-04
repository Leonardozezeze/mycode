#include "mainwindow.h"
#include "configstatuspanel.h"
#include "wavepanel.h"
#include <QLabel>
#include <QBoxLayout>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(900, 600);
    QWidget* m_central=createCentralWidget();
    setCentralWidget(m_central);
}
QWidget* MainWindow::createCentralWidget() {
    QWidget* m_central = new QWidget;
    QVBoxLayout *main_layout = new QVBoxLayout(m_central);
    main_layout->addWidget(new ConfigStatusPanel);
    main_layout->addWidget(new WavePanel);
    return m_central;
}

MainWindow::~MainWindow() = default;
