#include "serialpanel.h"
#include "logpanel.h"

#include <QBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSettings>
#include <QTimer>
#include <QSignalBlocker>
#include <QAbstractItemView>
#include <QtSerialPort/QSerialPortInfo>

namespace
{

// 状态色：用 QRgb 整数构造，避免 clazy-qcolor-from-literal 警告，
// 也避免每次字符串解析。值可以直接改这里统一配色。
constexpr QRgb kColorMuted = 0x9a9a9a;   // 未连接/灰色
constexpr QRgb kColorSuccess = 0x66bb6a; // 连接成功绿
constexpr QRgb kColorError = 0xef5350;   // 打开失败/断开红

QLabel *makeFormLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName("formLabel");
    return label;
}

QString parityLetter(QSerialPort::Parity parity)
{
    switch (parity)
    {
    case QSerialPort::NoParity:
        return QStringLiteral("N");
    case QSerialPort::EvenParity:
        return QStringLiteral("E");
    case QSerialPort::OddParity:
        return QStringLiteral("O");
    case QSerialPort::MarkParity:
        return QStringLiteral("M");
    case QSerialPort::SpaceParity:
        return QStringLiteral("S");
    }
    return QStringLiteral("?");
}

// 显示成 8N1 / 8E1 这种紧凑形式，有流控再加后缀
QString portSettingsSummary(QSerialPort::DataBits dataBits, QSerialPort::Parity parity,
                            QSerialPort::StopBits stopBits, QSerialPort::FlowControl flowControl)
{
    QString summary = QString::number(static_cast<int>(dataBits)) + parityLetter(parity);

    switch (stopBits)
    {
    case QSerialPort::OneStop:
        summary += QStringLiteral("1");
        break;
    case QSerialPort::OneAndHalfStop:
        summary += QStringLiteral("1.5");
        break;
    case QSerialPort::TwoStop:
        summary += QStringLiteral("2");
        break;
    }

    if (flowControl == QSerialPort::HardwareControl)
        summary += QStringLiteral(" RTS/CTS");
    else if (flowControl == QSerialPort::SoftwareControl)
        summary += QStringLiteral(" XON/XOFF");

    return summary;
}

} // namespace

SerialPanel::SerialPanel(QWidget *parent, PortProvider ports, const QString &settingsGroup)
    : QWidget(parent), m_portProvider(std::move(ports)), m_settingsGroup(settingsGroup)
{
    if (!m_portProvider)
        m_portProvider = []
        {
            QList<SerialDevice> devices;
            for (const auto &info : QSerialPortInfo::availablePorts())
                devices.append({info.portName(), info.description(), info.serialNumber(),
                                info.hasVendorIdentifier() ? int(info.vendorIdentifier()) : -1,
                                info.hasProductIdentifier() ? int(info.productIdentifier()) : -1});
            return devices;
        };
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(5);

    m_serial = new QSerialPort(this);

    m_portCombo = new QComboBox(this);
    m_portCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_portCombo->setMinimumContentsLength(10);
    m_portCombo->setObjectName("serialPortCombo");

    m_baudCombo = new QComboBox(this);
    m_baudCombo->setObjectName("serialBaudCombo");
    m_baudCombo->setEditable(true); // 允许手输非标准波特率
    m_baudCombo->addItems(
        {"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    m_baudCombo->setCurrentText("115200");

    m_showNativeCheck = new QCheckBox(tr("显示 ttyS*"), this);
    m_showNativeCheck->setChecked(false);

    m_refreshBtn = new QPushButton(tr("刷新"), this);

    m_openBtn = new QPushButton(tr("连接"), this);
    m_openBtn->setObjectName("primaryButton");
    m_openBtn->setMinimumHeight(28);

    m_stateLabel = new QLabel(this);
    m_stateLabel->setWordWrap(true);
    QFont stateFont = m_stateLabel->font();
    stateFont.setBold(true);
    m_stateLabel->setFont(stateFont);
    setStateLabel(tr("未连接"), QColor(kColorMuted));

    // 表单布局：标签右对齐，输入控件拉伸
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(5);

    // 端口行：下拉框 + 刷新，下面跟一行设备描述
    auto *portRow = new QWidget(this);
    auto *portColumn = new QVBoxLayout(portRow);
    portColumn->setContentsMargins(0, 0, 0, 0);
    portColumn->setSpacing(2);

    auto *portLine = new QHBoxLayout;
    portLine->setSpacing(6);
    portLine->addWidget(m_portCombo, 1);
    portLine->addWidget(m_refreshBtn);
    portColumn->addLayout(portLine);

    m_deviceDesc = new QLabel(QStringLiteral("—"), portRow);
    m_deviceDesc->setObjectName("deviceDesc");
    m_deviceDesc->setWordWrap(true);
    m_deviceDesc->setTextInteractionFlags(Qt::TextSelectableByMouse);
    portColumn->addWidget(m_deviceDesc);

    form->addRow(makeFormLabel(tr("端口"), this), portRow);

    auto *baudRow = new QWidget(this);
    auto *baudLayout = new QHBoxLayout(baudRow);
    baudLayout->setContentsMargins(0, 0, 0, 0);
    baudLayout->setSpacing(6);
    baudLayout->addWidget(m_baudCombo, 1);
    baudLayout->addWidget(m_showNativeCheck);
    form->addRow(makeFormLabel(tr("波特率"), this), baudRow);

    layout->addLayout(form);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(6);
    m_advancedBtn = new QPushButton(this);
    updateAdvancedButtonText();
    buttonRow->addWidget(m_advancedBtn);
    buttonRow->addWidget(m_openBtn, 1);
    layout->addLayout(buttonRow);

    auto *deviceRow = new QHBoxLayout;
    deviceRow->addWidget(m_stateLabel, 1);
    m_deviceId = new QLabel(this);
    m_deviceId->setObjectName("deviceId");
    deviceRow->addWidget(m_deviceId);
    layout->addLayout(deviceRow);
    m_autoReconnectCheck = new QCheckBox(tr("异常断开后自动重连"), this);
    m_autoReconnectCheck->setObjectName("autoReconnectCheck");
    m_autoReconnectCheck->setToolTip(tr("仅在本次已连接设备异常断开后重试；手动断开或取消后不再重试。无序列号的设备仅尝试原端口。"));
    layout->addWidget(m_autoReconnectCheck);

    auto *traffic = new QWidget(this);
    auto *trafficLayout = new QVBoxLayout(traffic);
    trafficLayout->setContentsMargins(0, 0, 0, 0);
    trafficLayout->setSpacing(2);
    m_trafficLabel = new QLabel(traffic);
    m_trafficLabel->setObjectName("serialTraffic");
    m_trafficLabel->setToolTip(
        tr("本次或最近一次连接的原始接收字节和实际写出字节；连接成功后清零。"));
    m_lastReceiveLabel = new QLabel(traffic);
    m_lastReceiveLabel->setObjectName("serialLastReceive");
    m_lastReceiveLabel->setToolTip(
        tr("最近收到原始串口字节的时间；有效波形帧的更新时间请看状态区。"));
    trafficLayout->addWidget(m_trafficLabel);
    trafficLayout->addWidget(m_lastReceiveLabel);
    layout->addWidget(traffic);
    refreshTraffic();

    layout->addStretch();

    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    m_preferredPort = settings.value("port").toString();
    m_selectionHint = m_preferredPort;
    m_lastDevice = {settings.value("devicePort", m_preferredPort).toString(), {}, settings.value("deviceSerial").toString(),
                    settings.value("deviceVendor", -1).toInt(),
                    settings.value("deviceProduct", -1).toInt()};
    m_autoReconnectCheck->setChecked(settings.value("autoReconnect", false).toBool());
    m_baudCombo->setCurrentText(settings.value("baud", "115200").toString());
    m_showNativeCheck->setChecked(settings.value("showNative", false).toBool());
    m_dataBits = static_cast<QSerialPort::DataBits>(
        settings.value("dataBits", int(QSerialPort::Data8)).toInt());
    m_parity = static_cast<QSerialPort::Parity>(
        settings.value("parity", int(QSerialPort::NoParity)).toInt());
    m_stopBits = static_cast<QSerialPort::StopBits>(
        settings.value("stopBits", int(QSerialPort::OneStop)).toInt());
    m_flowControl = static_cast<QSerialPort::FlowControl>(
        settings.value("flowControl", int(QSerialPort::NoFlowControl)).toInt());
    updateAdvancedButtonText();
#ifndef Q_OS_LINUX
    m_showNativeCheck->hide();
#endif
    refreshPorts();
    updateDeviceInfo();

    connect(m_openBtn, &QPushButton::clicked, this, &SerialPanel::onOpenClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &SerialPanel::onRefreshClicked);
    connect(m_showNativeCheck, &QCheckBox::toggled, this, &SerialPanel::refreshPorts);
    connect(m_serial, &QSerialPort::readyRead, this, &SerialPanel::onReadyRead);
    connect(this, &SerialPanel::dataReceived, this,
            [this](const QByteArray &data)
            {
                m_receivedBytes += quint64(data.size());
                m_lastReceivedAt = QDateTime::currentDateTime();
            });
    connect(m_serial, &QSerialPort::bytesWritten, this,
            [this](qint64 count)
            {
                if (count > 0)
                    m_transmittedBytes += quint64(count);
            });
    auto *trafficTimer = new QTimer(this);
    trafficTimer->setInterval(250);
    connect(trafficTimer, &QTimer::timeout, this, &SerialPanel::refreshTraffic);
    trafficTimer->start();
    connect(m_serial, &QSerialPort::errorOccurred, this, &SerialPanel::onErrorOccurred);
    connect(m_advancedBtn, &QPushButton::clicked, this, &SerialPanel::openAdvancedDialog);

    auto *portTimer = new QTimer(this);
    portTimer->setInterval(1500);
    connect(portTimer, &QTimer::timeout, this, &SerialPanel::pollPorts);
    portTimer->start();
    connect(m_autoReconnectCheck, &QCheckBox::toggled, this, [this](bool enabled)
    {
        QSettings settings;
        settings.beginGroup(m_settingsGroup);
        settings.setValue("autoReconnect", enabled);
        if (!enabled && m_waitingReconnect)
            cancelReconnect();
    });

    // 切换端口时刷新设备信息
    connect(m_portCombo, &QComboBox::currentIndexChanged, this, &SerialPanel::updateDeviceInfo);
    connect(m_portCombo, &QComboBox::currentIndexChanged, this, [this]
    {
        if (m_portCombo->currentIndex() >= 0)
            m_selectionHint = m_portCombo->currentData().toString();
    });
}

QString SerialPanel::matchingPort(const QList<SerialDevice> &ports, const SerialDevice &target)
{
    QString match;
    int matches = 0;
    for (const auto &device : ports)
    {
        const bool idsMatch = (target.vendor < 0 || target.vendor == device.vendor) &&
                              (target.product < 0 || target.product == device.product);
        const bool identityMatch = target.serialNumber.isEmpty()
            ? device.port == target.port
            : device.serialNumber == target.serialNumber;
        if (idsMatch && identityMatch)
        {
            match = device.port;
            ++matches;
        }
    }
    return matches == 1 ? match : QString();
}

void SerialPanel::refreshPorts()
{
    const QString selected = m_portCombo->currentData().toString();
    const QString currentPort = selected.isEmpty() ? m_selectionHint : selected;
    m_devices = m_portProvider();
    QList<SerialDevice> visible;
    for (const auto &device : m_devices)
        if (m_showNativeCheck->isChecked() || !device.port.startsWith("ttyS", Qt::CaseInsensitive))
            visible.append(device);
    QStringList names, labels;
    for (const auto &device : visible)
    {
        names << device.port;
        labels << (device.description.isEmpty() || device.description == device.port
                       ? device.port : device.port + " (" + device.description + ")");
    }
    bool changed = m_portCombo->count() != names.size();
    for (int i = 0; !changed && i < names.size(); ++i)
        changed = m_portCombo->itemData(i).toString() != names[i] ||
                  m_portCombo->itemText(i) != labels[i];
    if (changed || m_waitingReconnect)
    {
        const QSignalBlocker blocker(m_portCombo);
        if (changed)
        {
            m_portCombo->clear();
            for (int i = 0; i < names.size(); ++i)
                m_portCombo->addItem(labels[i], names[i]);
        }
        QString desired = m_waitingReconnect ? matchingPort(visible, m_lastDevice) : currentPort;
        if (selected.isEmpty() && currentPort == m_lastDevice.port && !m_lastDevice.serialNumber.isEmpty())
            desired = matchingPort(visible, m_lastDevice);
        if (!m_waitingReconnect && !names.contains(desired) && !m_lastDevice.port.isEmpty())
            desired = matchingPort(visible, m_lastDevice);
        const int index = names.indexOf(desired);
        // 没有历史选择时允许默认第一项；原设备消失后不跳到其他设备。
        m_portCombo->setCurrentIndex(index >= 0 ? index : currentPort.isEmpty() && !names.isEmpty() ? 0 : -1);
        if (m_portCombo->currentIndex() >= 0)
            m_selectionHint = m_portCombo->currentData().toString();
    }
    updateDeviceInfo();
}

void SerialPanel::pollPorts()
{
    if (isOpen() || m_portCombo->view()->isVisible())
        return;
    refreshPorts();
    if (m_waitingReconnect && !m_portCombo->currentData().toString().isEmpty())
        openSelectedPort(true);
}

void SerialPanel::cancelReconnect()
{
    m_waitingReconnect = false;
    m_openBtn->setText(tr("连接"));
    setConfigEnabled(true);
    setStateLabel(tr("未连接"), QColor(kColorMuted));
}

void SerialPanel::updateDeviceInfo()
{
    const QString name = m_portCombo->currentData().toString();
    SerialDevice device;
    for (const auto &entry : m_devices)
        if (entry.port == name)
            device = entry;
    m_deviceDesc->setText(name.isEmpty() ? QStringLiteral("—") :
                         device.description.isEmpty() ? name : device.description);
    m_deviceDesc->setToolTip(device.description);
    m_deviceId->setText(device.vendor >= 0 && device.product >= 0
        ? QStringLiteral("VID:PID  %1:%2").arg(device.vendor, 4, 16, QLatin1Char('0'))
            .arg(device.product, 4, 16, QLatin1Char('0')).toUpper()
        : QStringLiteral("VID:PID  —"));
    m_deviceId->setToolTip(device.serialNumber.isEmpty()
        ? tr("设备未提供序列号；自动重连只能尝试原端口，无法区分同型号替换设备。")
        : tr("设备序列号：%1\n重连时匹配唯一序列号及 VID/PID，可跟随端口号变化。")
            .arg(device.serialNumber));
}

void SerialPanel::onOpenClicked()
{
    if (m_waitingReconnect)
    {
        cancelReconnect();
        LogPanel::info(tr("已取消自动重连"));
        return;
    }
    if (m_serial->isOpen())
    {
        m_serial->close();
        m_openBtn->setText(tr("连接"));
        setStateLabel(tr("未连接"), QColor(kColorMuted));
        setConfigEnabled(true);
        emit connectionChanged(false);
        return;
    }

    openSelectedPort();
}

void SerialPanel::openSelectedPort(bool automatic)
{
    const QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty())
    {
        setStateLabel(tr("没有可用端口"), QColor(kColorError));
        LogPanel::warning(tr("没有可用串口，请检查设备连接或点击刷新"));
        return;
    }

    bool validBaud = false;
    const int baud = m_baudCombo->currentText().toInt(&validBaud);
    if (!validBaud || baud <= 0)
    {
        setStateLabel(tr("波特率无效"), QColor(kColorError));
        LogPanel::warning(tr("请输入有效的正整数波特率。"));
        return;
    }
    m_serial->setPortName(portName);
    if (!m_serial->setBaudRate(baud) || !m_serial->setDataBits(m_dataBits) ||
        !m_serial->setParity(m_parity) || !m_serial->setStopBits(m_stopBits) ||
        !m_serial->setFlowControl(m_flowControl))
    {
        setStateLabel(tr("串口参数无效"), QColor(kColorError));
        LogPanel::error(tr("设置串口参数失败：%1").arg(m_serial->errorString()));
        return;
    }

    if (m_serial->open(QIODevice::ReadWrite))
    {
        m_waitingReconnect = false;
        m_lastOpenError.clear();
        m_lastDevice = {portName, {}, {}, -1, -1};
        for (const auto &device : m_devices)
            if (device.port == portName)
                m_lastDevice = device;
        m_receivedBytes = m_transmittedBytes = 0;
        m_lastReceivedAt = QDateTime();
        refreshTraffic();
        m_openBtn->setText(tr("断开"));
        setStateLabel(tr("已连接"), QColor(kColorSuccess));
        setConfigEnabled(false);
        m_preferredPort = portName;
        saveSettings();
        emit connectionChanged(true);
        if (automatic)
            LogPanel::info(tr("原设备已重新连接：%1").arg(portName));
    }
    else
    {
        setStateLabel(automatic ? tr("重连失败，继续等待") : tr("连接失败"), QColor(kColorError));
        const auto reason = m_serial->errorString();
        if (!automatic || reason != m_lastOpenError)
            LogPanel::error(tr("串口 %1 打开失败：%2").arg(portName, reason));
        m_lastOpenError = reason;
    }
}

void SerialPanel::refreshTraffic()
{
    auto formatBytes = [](quint64 bytes)
    {
        if (bytes < 1024)
            return QString::number(bytes) + QStringLiteral(" B");
        if (bytes < 1024 * 1024)
            return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
    };
    m_trafficLabel->setText(
        tr("接收 %1  ·  发送 %2")
            .arg(formatBytes(m_receivedBytes), formatBytes(m_transmittedBytes)));
    m_lastReceiveLabel->setText(tr("最近接收  %1")
                                    .arg(m_lastReceivedAt.isValid()
                                             ? m_lastReceivedAt.toString("HH:mm:ss.zzz")
                                             : QStringLiteral("--")));
}

void SerialPanel::onRefreshClicked()
{
    refreshPorts();
}

bool SerialPanel::isOpen() const
{
    return m_serial && m_serial->isOpen();
}

qint64 SerialPanel::sendData(const QByteArray &data)
{
    if (!isOpen())
        return -1;
    return m_serial->write(data);
}

void SerialPanel::onReadyRead()
{
    const QByteArray data = m_serial->readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialPanel::setStateLabel(const QString &text, const QColor &color)
{
    m_stateLabel->setText(QStringLiteral("● ") + text);
    m_stateLabel->setStyleSheet(QStringLiteral("color: %1;").arg(color.name()));
}

void SerialPanel::setConfigEnabled(bool enabled)
{
    m_portCombo->setEnabled(enabled);
    m_baudCombo->setEnabled(enabled);
    m_refreshBtn->setEnabled(enabled);
    m_showNativeCheck->setEnabled(enabled);
    m_advancedBtn->setEnabled(enabled);
}

void SerialPanel::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    // 设备被拔掉 / 资源消失：主动关闭并同步界面状态
    if (error == QSerialPort::ResourceError && m_serial->isOpen())
    {
        const QString reason = m_serial->errorString();
        m_serial->close();
        m_openBtn->setText(tr("连接"));
        setStateLabel(tr("连接断开"), QColor(kColorError));
        setConfigEnabled(true);
        m_waitingReconnect = m_autoReconnectCheck->isChecked();
        if (m_waitingReconnect)
        {
            m_openBtn->setText(tr("取消重连"));
            setConfigEnabled(false);
            setStateLabel(tr("等待原设备重连"), QColor(kColorError));
        }
        emit connectionChanged(false);
        LogPanel::error(tr("串口异常断开：%1").arg(reason));
        return;
    }

    // 连接期间的其他错误只记录，不强行关闭
    if (m_serial->isOpen())
        LogPanel::warning(tr("串口错误：%1").arg(m_serial->errorString()));
}

void SerialPanel::openAdvancedDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("高级串口选项"));

    auto *form = new QFormLayout(&dlg);

    auto *dataBitsCombo = new QComboBox(&dlg);
    dataBitsCombo->addItem(QStringLiteral("5"), int(QSerialPort::Data5));
    dataBitsCombo->addItem(QStringLiteral("6"), int(QSerialPort::Data6));
    dataBitsCombo->addItem(QStringLiteral("7"), int(QSerialPort::Data7));
    dataBitsCombo->addItem(QStringLiteral("8"), int(QSerialPort::Data8));
    dataBitsCombo->setCurrentIndex(dataBitsCombo->findData(int(m_dataBits)));

    auto *parityCombo = new QComboBox(&dlg);
    parityCombo->addItem(tr("无"), int(QSerialPort::NoParity));
    parityCombo->addItem(tr("偶校验"), int(QSerialPort::EvenParity));
    parityCombo->addItem(tr("奇校验"), int(QSerialPort::OddParity));
    parityCombo->addItem(tr("标记"), int(QSerialPort::MarkParity));
    parityCombo->addItem(tr("空格"), int(QSerialPort::SpaceParity));
    parityCombo->setCurrentIndex(parityCombo->findData(int(m_parity)));

    auto *stopBitsCombo = new QComboBox(&dlg);
    stopBitsCombo->addItem(QStringLiteral("1"), int(QSerialPort::OneStop));
    stopBitsCombo->addItem(QStringLiteral("1.5"), int(QSerialPort::OneAndHalfStop));
    stopBitsCombo->addItem(QStringLiteral("2"), int(QSerialPort::TwoStop));
    stopBitsCombo->setCurrentIndex(stopBitsCombo->findData(int(m_stopBits)));

    auto *flowCombo = new QComboBox(&dlg);
    flowCombo->addItem(tr("无"), int(QSerialPort::NoFlowControl));
    flowCombo->addItem(tr("硬件 (RTS/CTS)"), int(QSerialPort::HardwareControl));
    flowCombo->addItem(tr("软件 (XON/XOFF)"), int(QSerialPort::SoftwareControl));
    flowCombo->setCurrentIndex(flowCombo->findData(int(m_flowControl)));

    form->addRow(makeFormLabel(tr("数据位"), &dlg), dataBitsCombo);
    form->addRow(makeFormLabel(tr("校验位"), &dlg), parityCombo);
    form->addRow(makeFormLabel(tr("停止位"), &dlg), stopBitsCombo);
    form->addRow(makeFormLabel(tr("流控"), &dlg), flowCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    m_dataBits = static_cast<QSerialPort::DataBits>(dataBitsCombo->currentData().toInt());
    m_parity = static_cast<QSerialPort::Parity>(parityCombo->currentData().toInt());
    m_stopBits = static_cast<QSerialPort::StopBits>(stopBitsCombo->currentData().toInt());
    m_flowControl = static_cast<QSerialPort::FlowControl>(flowCombo->currentData().toInt());

    updateAdvancedButtonText();
    saveSettings();
}

SerialPanel::~SerialPanel()
{
    saveSettings();
}

void SerialPanel::saveSettings()
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    const auto selected = m_portCombo->currentData().toString();
    settings.setValue("port", selected.isEmpty() ? m_preferredPort : selected);
    bool ok = false;
    const int baud = m_baudCombo->currentText().toInt(&ok);
    if (ok && baud > 0)
        settings.setValue("baud", baud);
    settings.setValue("devicePort", m_lastDevice.port);
    settings.setValue("deviceSerial", m_lastDevice.serialNumber);
    settings.setValue("deviceVendor", m_lastDevice.vendor);
    settings.setValue("deviceProduct", m_lastDevice.product);
    settings.setValue("autoReconnect", m_autoReconnectCheck->isChecked());
    settings.setValue("showNative", m_showNativeCheck->isChecked());
    settings.setValue("dataBits", int(m_dataBits));
    settings.setValue("parity", int(m_parity));
    settings.setValue("stopBits", int(m_stopBits));
    settings.setValue("flowControl", int(m_flowControl));
}

void SerialPanel::updateAdvancedButtonText()
{
    m_advancedBtn->setText(
        tr("高级选项 (%1)")
            .arg(portSettingsSummary(m_dataBits, m_parity, m_stopBits, m_flowControl)));
}
