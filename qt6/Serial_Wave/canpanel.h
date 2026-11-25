#ifndef CANPANEL_H
#define CANPANEL_H

#include <QList>
#include <QTime>
#include <QWidget>

class QLabel;
class QLineEdit;
class QProcess;
class QPushButton;
class QShowEvent;
class QSocketNotifier;

class CanPanel : public QWidget
{
    Q_OBJECT
public:
    enum class Role { Sensor, Console };

    explicit CanPanel(QWidget *parent = nullptr, Role role = Role::Sensor);
    ~CanPanel() override;
    QString sendCommand(const QByteArray &payload);

signals:
    void frameReceived(quint32 id, const QByteArray &payload);
    void connectionChanged(bool connected);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void toggleConnection();
    void readFrames();
    void closeSocket();
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
    QLabel *m_receiveIdStatus = nullptr;
    QPushButton *m_connect = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_interfaceState = nullptr;
    QLabel *m_bitrate = nullptr;
    QLabel *m_traffic = nullptr;
    QLabel *m_lastReceived = nullptr;
    QProcess *m_linkQuery = nullptr;
    QString m_queriedInterface;
    QSocketNotifier *m_notifier = nullptr;
    int m_socket = -1;
    quint64 m_receivedFrames = 0;
    quint64 m_receivedBytes = 0;
    QTime m_lastReceivedAt;
};

#endif // CANPANEL_H
