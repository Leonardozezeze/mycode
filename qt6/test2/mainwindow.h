#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private:
    QWidget *createCentralWidget();
    QWidget *config_statusWidget();
    QWidget *WaveWidget();
};
#endif // MAINWINDOW_H
