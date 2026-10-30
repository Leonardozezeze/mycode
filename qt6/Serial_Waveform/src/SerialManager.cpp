#include "SerialManager.h"
#include <QSerialPortInfo>
#include <QDir>
#include <QDebug>

SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
{
    connect(m_serial, &QSerialPort::readyRead,
            this, &SerialManager::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, &SerialManager::onSerialError);
}

SerialManager::~SerialManager()
{
    close();
}

bool SerialManager::open(const QString &portName, qint32 baudRate)
{
    if (m_serial->isOpen()) {
        close();
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(baudRate);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit errorOccurred(tr("无法打开串口 %1: %2").arg(portName, m_serial->errorString()));
        emit connectionChanged(false);
        return false;
    }

    m_receiveBuffer.clear();
    emit connectionChanged(true);
    return true;
}

void SerialManager::close()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        m_receiveBuffer.clear();
        emit connectionChanged(false);
    }
}

bool SerialManager::isOpen() const
{
    return m_serial->isOpen();
}

QStringList SerialManager::availablePorts()
{
    QStringList ports;

    // 1. 标准 Qt 扫描
    const auto infos = QSerialPortInfo::availablePorts();
    for (const auto &info : infos) {
        ports << info.portName();
    }

    // 2. 额外扫描 /dev/ttyV* (虚拟串口)
    QStringList vports = scanSerialPorts("/dev/ttyV*");
    for (const auto &p : vports) {
        if (!ports.contains(p)) {
            ports << p;
        }
    }

    // 3. 额外扫描 /dev/ttyUSB* (USB 转串口，有时 Qt 检测不到)
    QStringList usbports = scanSerialPorts("/dev/ttyUSB*");
    for (const auto &p : usbports) {
        if (!ports.contains(p)) {
            ports << p;
        }
    }

    ports.sort();
    return ports;
}

QStringList SerialManager::scanSerialPorts(const QString &pathPattern)
{
    QStringList result;
    QDir dir("/dev");
    QString pattern = pathPattern;
    if (pattern.startsWith("/dev/")) {
        pattern = pattern.mid(5); // 去掉 "/dev/" 前缀
    }

    const auto entries = dir.entryList(QStringList() << pattern, QDir::System);
    for (const auto &entry : entries) {
        result << "/dev/" + entry;
    }
    return result;
}

void SerialManager::onReadyRead()
{
    m_receiveBuffer.append(m_serial->readAll());

    // 按行解析
    while (true) {
        int idx = m_receiveBuffer.indexOf('\n');
        if (idx < 0) break;

        QByteArray line = m_receiveBuffer.left(idx);
        m_receiveBuffer.remove(0, idx + 1);

        // 去掉 \r
        if (!line.isEmpty() && line.endsWith('\r')) {
            line.chop(1);
        }

        if (!line.isEmpty()) {
            parseLine(line);
        }
    }
}

void SerialManager::parseLine(const QByteArray &line)
{
    // 协议：timestamp,signal,grain_count,loss_rate,fan_rpm
    //   timestamp: 时间戳(ms)
    //   signal: 压电传感器信号值
    //   grain_count: 籽粒数量
    //   loss_rate: 损失率(%)
    //   fan_rpm: 风机转速(RPM)

    QString str = QString::fromUtf8(line).trimmed();
    if (str.isEmpty()) return;

    QStringList parts = str.split(',');
    bool ok = false;

    if (parts.size() >= 5) {
        // 标准5字段协议
        quint64 ts = parts[0].toULongLong(&ok);
        if (!ok) return;

        double signal = parts[1].toDouble(&ok);
        if (!ok) return;

        double grainCount = parts[2].toDouble(&ok);
        if (!ok) return;

        double lossRate = parts[3].toDouble(&ok);
        if (!ok) return;

        double fanRpm = parts[4].toDouble(&ok);
        if (!ok) return;

        emit dataPointReceived(ts, signal, grainCount, lossRate, fanRpm);
    } else if (parts.size() == 2) {
        // 兼容旧协议: timestamp,value
        quint64 ts = parts[0].toULongLong(&ok);
        if (!ok) return;
        double val = parts[1].toDouble(&ok);
        if (ok) {
            emit dataPointReceived(ts, val, 0, 0, 0);
        }
    } else if (parts.size() == 1) {
        // 兼容旧协议: value
        double val = parts[0].toDouble(&ok);
        if (ok) {
            static quint64 autoTimestamp = 0;
            autoTimestamp += 10;
            emit dataPointReceived(autoTimestamp, val, 0, 0, 0);
        }
    }
}

void SerialManager::onSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) return;

    emit errorOccurred(m_serial->errorString());

    if (error == QSerialPort::ResourceError) {
        close();
    }
}
