#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <memory>

class SerialPanel;
class CommandBar;
class WavePanel;
class RecordingPanel;
class StatusPanel;
class ProtocolParser;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    QWidget *createCentralWidget();
    QWidget *createCSWaveWidget();
    QWidget *createConsoleLogWidget();
    void autoExportRecording();

    SerialPanel    *m_serialPanel    = nullptr;
    CommandBar     *m_commandBar     = nullptr;
    WavePanel      *m_wavePanel      = nullptr;
    RecordingPanel *m_recordingPanel = nullptr;
    StatusPanel    *m_statusPanel    = nullptr;

    std::unique_ptr<ProtocolParser> m_parser;
};
#endif // MAINWINDOW_H
