#ifndef CANPANEL_H
#define CANPANEL_H

#include <QList>
#include <QTime>
#include <QWidget>

class QCanBusDevice;
class QLabel;
class QLineEdit;
class QProcess;
class QPushButton;
class QShowEvent;
class QTimer;

class CanPanel : public QWidget
{
    Q_OBJECT
public:
    enum class Role { Sensor, Console };

    explicit CanPanel(QWidget *parent = nullptr, Role role = Role::Sensor);
    ~CanPanel() override;
    QString sendCommand(quint32 id, const QByteArray &payload);

signals:
    void frameReceived(quint32 id, const QByteArray &payload);
    void connectionChanged(bool connected);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void toggleConnection();
    void handleConnected();
    void handleUnconnected();
    void readFrames();
    void closeDevice();
    void disconnectDevice();
    void showError(const QString &message);
    void refreshInterface();
    void refreshTraffic();
    void refreshReceiveIds();
    QList<quint32> receiveIds(QString &error) const;
    Role m_role;
    QString m_settingsGroup;
    QLineEdit *m_interface = nullptr;
    QLineEdit *m_canId = nullptr;
    QLineEdit *m_bitrateInput = nullptr; // 非 Linux：PCAN 波特率由程序设置
    QLabel *m_receiveIdStatus = nullptr;
    QPushButton *m_connect = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_interfaceState = nullptr;
    QLabel *m_bitrate = nullptr;
    QLabel *m_traffic = nullptr;
    QLabel *m_lastReceived = nullptr;
    QProcess *m_linkQuery = nullptr;
    QString m_queriedInterface;
    QCanBusDevice *m_device = nullptr;
    QTimer *m_receiveContinuation = nullptr;
    bool m_connected = false;
    QList<quint32> m_filterIds;
    quint64 m_receivedFrames = 0;
    quint64 m_receivedBytes = 0;
    QTime m_lastReceivedAt;
};

#endif // CANPANEL_H
