#ifndef STATUSPANEL_H
#define STATUSPANEL_H

#include <QWidget>
#include <QColor>
#include <QElapsedTimer>

#include "machinestatus.h"

class QLabel;
class QFrame;
class QProgressBar;

// 状态区：只显示数据，不解析协议。
// 数据层调用 updateStatus() 刷新即可。
class StatusPanel : public QWidget
{
    Q_OBJECT
public:
    explicit StatusPanel(QWidget *parent = nullptr);

public slots:
    void updateStatus(const MachineStatus &status);
    void reset();
    void setConnected(bool connected);
    void setReplayMode(bool playback);
    void refreshFreshness();

private:
    QFrame *createCard(const QString &title, QLabel **valueLabel);
    QFrame *createStatItem(const QString &key, QLabel **valueLabel);
    void setValue(QLabel *label, const QString &text, const QColor &color = QColor());

    // 品质
    QLabel *m_lblCrushRate = nullptr;
    QLabel *m_lblCrushAvg = nullptr;
    QLabel *m_lblImpurityRate = nullptr;
    QLabel *m_lblImpurityAvg = nullptr;

    // 电机与推杆
    QLabel *m_lblRpm = nullptr;
    QLabel *m_lblRpmTarget = nullptr;
    QLabel *m_lblRpmError = nullptr;
    QLabel *m_lblRunState = nullptr;
    QLabel *m_lblActuatorPosition = nullptr;
    QLabel *m_lblActuatorState = nullptr;
    QProgressBar *m_actuatorProgress = nullptr;

    // 第三组：通信统计
    QLabel *m_lblFrames = nullptr;
    QLabel *m_lblErrors = nullptr;
    QLabel *m_lblHz = nullptr;
    QLabel *m_lblLastUpdate = nullptr;
    QElapsedTimer m_lastReceived;
    bool m_connected = false;
    bool m_hasData = false;
    bool m_playback = false;
    MachineStatus m_latest;
};

#endif // STATUSPANEL_H
