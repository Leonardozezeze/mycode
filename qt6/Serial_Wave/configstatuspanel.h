#ifndef CONFIGSTATUSPANEL_H
#define CONFIGSTATUSPANEL_H

#include <QWidget>

class SerialPanel;
class StatusPanel;
class RecordingPanel;

class ConfigStatusPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ConfigStatusPanel(QWidget *parent = nullptr);

    // 主窗口需要用它来把串口和控制台连起来
    SerialPanel *serialPanel() const;
    // 解析层拿到数据后调用 updateStatus()
    StatusPanel *statusPanel() const;
    // 主窗口把 WavePanel 注入进来，并接上导出按钮
    RecordingPanel *recordingPanel() const;

signals:

private:
    QWidget* createConfigWidget();
    QWidget* createStatusWidget();

    SerialPanel    *m_serialPanel    = nullptr;
    StatusPanel    *m_statusPanel    = nullptr;
    RecordingPanel *m_recordingPanel = nullptr;
};

#endif // CONFIGSTATUSPANEL_H
