#ifndef CONFIGSTATUSPANEL_H
#define CONFIGSTATUSPANEL_H

#include <QWidget>

class SerialPanel;
class CanPanel;
class StatusPanel;
class RecordingPanel;

class ConfigStatusPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigStatusPanel(QWidget *parent = nullptr);

    SerialPanel *serialPanel() const;
    CanPanel *canPanel() const;
    SerialPanel *consoleSerialPanel() const;
    CanPanel *consoleCanPanel() const;
    // 解析层拿到数据后调用 updateStatus()
    StatusPanel *statusPanel() const;
    // 主窗口把 WavePanel 注入进来，并接上导出按钮
    RecordingPanel *recordingPanel() const;

private:
    QWidget* createConfigWidget();
    QWidget* createSensorConfig();
    QWidget* createStatusWidget();

    SerialPanel    *m_serialPanel    = nullptr;
    CanPanel       *m_canPanel       = nullptr;
    SerialPanel    *m_consoleSerialPanel = nullptr;
    CanPanel       *m_consoleCanPanel = nullptr;
    StatusPanel    *m_statusPanel    = nullptr;
    RecordingPanel *m_recordingPanel = nullptr;
};

#endif // CONFIGSTATUSPANEL_H
