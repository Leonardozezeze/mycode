#pragma once

#include <QObject>
#include <QSerialPort>
#include <QString>
#include <QByteArray>
#include <QTimer>

// 串口数据管理器
// 支持 ASCII 协议：每行 "timestamp,value\n" 或 "value\n"
class SerialManager : public QObject
{
    Q_OBJECT
public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager();

    // 打开/关闭串口
    bool open(const QString &portName, qint32 baudRate);
    void close();
    bool isOpen() const;

    // 获取可用串口列表
    static QStringList availablePorts();

    // 扫描指定路径下的串口
    static QStringList scanSerialPorts(const QString &pathPattern = "/dev/tty*");

signals:
    // 收到一个数据点：时间戳(ms), 信号值, 籽粒数量, 损失率, 风机转速
    void dataPointReceived(quint64 timestamp, double signal, double grainCount, double lossRate, double fanRpm);

    // 错误信息
    void errorOccurred(const QString &error);

    // 连接状态变化
    void connectionChanged(bool connected);

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError error);

private:
    void parseLine(const QByteArray &line);

    QSerialPort *m_serial = nullptr;
    QByteArray m_receiveBuffer;
};
