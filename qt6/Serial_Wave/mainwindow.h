#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QStringList>
#include <memory>
class CommandBar;
class WavePanel;
class RecordingPanel;
class StatusPanel;
class ProtocolParser;
class QDockWidget;
class QCloseEvent;
class QAction;
class QToolBar;
class LogPanel;
class QTimer;
class QShowEvent;
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void resetLayout();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void autoExportRecording();
    void appendDeviceText(const QByteArray &bytes);
    void flushDeviceText();
    void queueRawReceive(const QByteArray &bytes);
    void queueCanReceive(quint32 id, const QByteArray &payload);
    void flushRawReceive();
    void clearReceiveBatch();
    void logConsoleTransmission(const QString &message);
    CommandBar *m_commandBar = nullptr;
    WavePanel *m_wavePanel = nullptr;
    RecordingPanel *m_recordingPanel = nullptr;
    StatusPanel *m_statusPanel = nullptr;
    QDockWidget *m_logDock = nullptr;
    QAction *m_consoleAction = nullptr;
    QToolBar *m_viewToolbar = nullptr;
    LogPanel *m_logPanel = nullptr;
    QTimer *m_rawReceiveTimer = nullptr;
    QTimer *m_textReceiveTimer = nullptr;
    QByteArray m_rawReceiveBuffer;
    quint64 m_rawReceiveOmitted = 0;
    QStringList m_canReceiveBatch;
    quint64 m_canReceiveOmitted = 0;
    bool m_sensorConnected = false;
    bool m_applyDefaultConsoleWidth = true;
    QByteArray m_rxText;
    std::unique_ptr<ProtocolParser> m_parser;
};
#endif
