#ifndef SERIALPANEL_H
#define SERIALPANEL_H

#include <QWidget>
#include <QColor>
#include <QtSerialPort/QSerialPort>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QDateTime>
#include <functional>

struct SerialDevice
{
    QString port;
    QString description;
    QString serialNumber;
    int vendor = -1;
    int product = -1;
};

class SerialPanel : public QWidget
{
    Q_OBJECT
public:
    using PortProvider = std::function<QList<SerialDevice>()>;
    explicit SerialPanel(QWidget *parent = nullptr, PortProvider ports = {},
                         const QString &settingsGroup = QStringLiteral("serial"));
    ~SerialPanel() override;
    bool isOpen() const;
    quint64 receivedBytes() const { return m_receivedBytes; }
    quint64 transmittedBytes() const { return m_transmittedBytes; }

    // 供控制台发送数据；串口未打开返回 -1
    qint64 sendData(const QByteArray &data);
    // 唯一序列号优先；无序列号只匹配原端口，绝不按 VID/PID 猜设备。
    static QString matchingPort(const QList<SerialDevice> &ports, const SerialDevice &target);
    bool isWaitingForReconnect() const { return m_waitingReconnect; }
    void cancelReconnect();

signals:
    void dataReceived(const QByteArray &data); // 原始字节，交给解析层
    void connectionChanged(bool connected);

private slots:
    void onOpenClicked();
    void onRefreshClicked();
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void pollPorts();

private:
    void refreshPorts();
    void updateDeviceInfo();
    void openAdvancedDialog();
    void updateAdvancedButtonText();
    void setStateLabel(const QString &text, const QColor &color);
    void setConfigEnabled(bool enabled);
    void saveSettings();
    void refreshTraffic();
    void openSelectedPort(bool automatic = false);

    QSerialPort *m_serial = nullptr;
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_advancedBtn = nullptr;
    QPushButton *m_openBtn = nullptr;
    QLabel *m_stateLabel = nullptr;
    QCheckBox *m_showNativeCheck = nullptr;
    QCheckBox *m_autoReconnectCheck = nullptr;
    PortProvider m_portProvider;
    QString m_settingsGroup;
    QList<SerialDevice> m_devices;
    SerialDevice m_lastDevice;
    bool m_waitingReconnect = false;
    QString m_lastOpenError;

    // 高级串口参数（默认 8N1）
    QSerialPort::DataBits m_dataBits = QSerialPort::Data8;
    QSerialPort::Parity m_parity = QSerialPort::NoParity;
    QSerialPort::StopBits m_stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl m_flowControl = QSerialPort::NoFlowControl;

    // 设备信息（端口描述显示在下拉框下面；VID:PID 显示在连接状态下面）
    QLabel *m_deviceDesc = nullptr;
    QLabel *m_deviceId = nullptr;
    QString m_preferredPort;
    QString m_selectionHint;
    QLabel *m_trafficLabel = nullptr;
    QLabel *m_lastReceiveLabel = nullptr;
    quint64 m_receivedBytes = 0;
    quint64 m_transmittedBytes = 0;
    QDateTime m_lastReceivedAt;
};

#endif // SERIALPANEL_H
