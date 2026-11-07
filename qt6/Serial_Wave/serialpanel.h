#ifndef SERIALPANEL_H
#define SERIALPANEL_H

#include <QWidget>
#include <QColor>
#include <QtSerialPort/QSerialPort>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>

class SerialPanel : public QWidget
{
    Q_OBJECT
public:
    explicit SerialPanel(QWidget *parent = nullptr);
    bool isOpen() const;

    // 供控制台发送数据；串口未打开返回 -1
    qint64 sendData(const QByteArray &data);

signals:
    void dataReceived(const QByteArray &data);   // 原始字节，交给解析层
    void connectionChanged(bool connected);

private slots:
    void onOpenClicked();
    void onRefreshClicked();
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    void refreshPorts();
    void updateDeviceInfo();
    void openAdvancedDialog();
    void updateAdvancedButtonText();
    void setStateLabel(const QString &text, const QColor &color);
    void setConfigEnabled(bool enabled);

    QSerialPort *m_serial          = nullptr;
    QComboBox   *m_portCombo       = nullptr;
    QComboBox   *m_baudCombo       = nullptr;
    QPushButton *m_refreshBtn      = nullptr;
    QPushButton *m_advancedBtn     = nullptr;
    QPushButton *m_openBtn         = nullptr;
    QLabel      *m_stateLabel      = nullptr;
    QCheckBox   *m_showNativeCheck = nullptr;

    // 高级串口参数（默认 8N1）
    QSerialPort::DataBits    m_dataBits    = QSerialPort::Data8;
    QSerialPort::Parity      m_parity      = QSerialPort::NoParity;
    QSerialPort::StopBits    m_stopBits    = QSerialPort::OneStop;
    QSerialPort::FlowControl m_flowControl = QSerialPort::NoFlowControl;

    // 设备信息（端口描述显示在下拉框下面；VID:PID 显示在连接状态下面）
    QLabel      *m_deviceDesc      = nullptr;
    QLabel      *m_deviceId        = nullptr;
};

#endif // SERIALPANEL_H
