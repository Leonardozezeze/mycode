#include "serialpanel.h"
#include "logpanel.h"

#include <QBoxLayout>
#include <QFormLayout>
#include <QFont>
#include <QDialog>
#include <QDialogButtonBox>
#include <QtSerialPort/QSerialPortInfo>

namespace {

// 状态色：用 QRgb 整数构造，避免 clazy-qcolor-from-literal 警告，
// 也避免每次字符串解析。值可以直接改这里统一配色。
constexpr QRgb kColorMuted   = 0x9a9a9a;   // 未连接/灰色
constexpr QRgb kColorSuccess = 0x66bb6a;   // 连接成功绿
constexpr QRgb kColorError   = 0xef5350;   // 打开失败/断开红

QLabel *makeFormLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName("formLabel");
    return label;
}

QString parityLetter(QSerialPort::Parity parity)
{
    switch (parity) {
    case QSerialPort::NoParity:    return QStringLiteral("N");
    case QSerialPort::EvenParity:  return QStringLiteral("E");
    case QSerialPort::OddParity:   return QStringLiteral("O");
    case QSerialPort::MarkParity:  return QStringLiteral("M");
    case QSerialPort::SpaceParity: return QStringLiteral("S");
    }
    return QStringLiteral("?");
}

// 显示成 8N1 / 8E1 这种紧凑形式，有流控再加后缀
QString portSettingsSummary(QSerialPort::DataBits dataBits,
                            QSerialPort::Parity parity,
                            QSerialPort::StopBits stopBits,
                            QSerialPort::FlowControl flowControl)
{
    QString summary = QString::number(static_cast<int>(dataBits))
                      + parityLetter(parity);

    switch (stopBits) {
    case QSerialPort::OneStop:        summary += QStringLiteral("1");   break;
    case QSerialPort::OneAndHalfStop: summary += QStringLiteral("1.5"); break;
    case QSerialPort::TwoStop:        summary += QStringLiteral("2");   break;
    }

    if (flowControl == QSerialPort::HardwareControl)
        summary += QStringLiteral(" RTS/CTS");
    else if (flowControl == QSerialPort::SoftwareControl)
        summary += QStringLiteral(" XON/XOFF");

    return summary;
}

} // namespace

SerialPanel::SerialPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("串口配置"), this);
    title->setObjectName("panelTitle");
    layout->addWidget(title);

    m_serial = new QSerialPort(this);

    m_portCombo = new QComboBox(this);

    m_baudCombo = new QComboBox(this);
    m_baudCombo->setEditable(true);   // 允许手输非标准波特率
    m_baudCombo->addItems({"9600", "19200", "38400", "57600",
                           "115200", "230400", "460800", "921600"});
    m_baudCombo->setCurrentText("115200");

    m_showNativeCheck = new QCheckBox(tr("显示 ttyS*"), this);
    m_showNativeCheck->setChecked(false);

    m_refreshBtn = new QPushButton(tr("刷新"), this);

    m_openBtn = new QPushButton(tr("连接"), this);
    m_openBtn->setObjectName("primaryButton");
    m_openBtn->setMinimumHeight(28);

    m_stateLabel = new QLabel(this);
    QFont stateFont = m_stateLabel->font();
    stateFont.setBold(true);
    m_stateLabel->setFont(stateFont);
    setStateLabel(tr("未连接"), QColor(kColorMuted));

    // 表单布局：标签右对齐，输入控件拉伸
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

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

    layout->addWidget(m_stateLabel);

    m_deviceId = new QLabel(this);
    m_deviceId->setObjectName("deviceId");
    layout->addWidget(m_deviceId);

    layout->addStretch();

    refreshPorts();
    updateDeviceInfo();

    connect(m_openBtn, &QPushButton::clicked,
            this, &SerialPanel::onOpenClicked);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &SerialPanel::onRefreshClicked);
    connect(m_showNativeCheck, &QCheckBox::toggled,
            this, &SerialPanel::refreshPorts);
    connect(m_serial, &QSerialPort::readyRead,
            this, &SerialPanel::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred,
            this, &SerialPanel::onErrorOccurred);
    connect(m_advancedBtn, &QPushButton::clicked,
            this, &SerialPanel::openAdvancedDialog);

    // 切换端口时刷新设备信息
    connect(m_portCombo, &QComboBox::currentIndexChanged,
            this, &SerialPanel::updateDeviceInfo);
}

void SerialPanel::refreshPorts()
{
    const QString currentPort = m_portCombo->currentData().toString();
    m_portCombo->clear();

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        const QString name = info.portName();
        const bool isNative = name.startsWith("ttyS", Qt::CaseInsensitive);
        if (isNative && !m_showNativeCheck->isChecked())
            continue;

        QString label = name;
        if (!info.description().isEmpty() && info.description() != name)
            label += QStringLiteral(" (%1)").arg(info.description());
        m_portCombo->addItem(label, name);   // userData 存纯设备名
    }

    // 刷新后尽量保留之前选中的端口
    const int idx = m_portCombo->findData(currentPort);
    if (idx >= 0)
        m_portCombo->setCurrentIndex(idx);

    updateDeviceInfo();
}

void SerialPanel::updateDeviceInfo()
{
    const QString name = m_portCombo->currentData().toString();
    const QSerialPortInfo info(name);

    if (name.isEmpty() || info.isNull()) {
        m_deviceDesc->setText(QStringLiteral("—"));
        m_deviceDesc->setToolTip(QString());
        m_deviceId->setText(QStringLiteral("VID:PID  —"));
        return;
    }

    m_deviceDesc->setText(info.description().isEmpty()
                          ? name : info.description());
    m_deviceDesc->setToolTip(info.description());

    if (info.hasVendorIdentifier() && info.hasProductIdentifier()) {
        m_deviceId->setText(
            QStringLiteral("VID:PID  %1:%2")
                .arg(info.vendorIdentifier(), 4, 16, QLatin1Char('0'))
                .arg(info.productIdentifier(), 4, 16, QLatin1Char('0'))
                .toUpper());
    } else {
        m_deviceId->setText(QStringLiteral("VID:PID  —"));
    }
}

void SerialPanel::onOpenClicked()
{
    if (m_serial->isOpen()) {
        m_serial->close();
        m_openBtn->setText(tr("连接"));
        setStateLabel(tr("未连接"), QColor(kColorMuted));
        setConfigEnabled(true);
        emit connectionChanged(false);
        return;
    }

    const QString portName = m_portCombo->currentData().toString();
    if (portName.isEmpty()) {
        setStateLabel(tr("没有可用端口"), QColor(kColorError));
        LogPanel::warning(tr("没有可用串口，请检查设备连接或点击刷新"));
        return;
    }

    m_serial->setPortName(portName);
    m_serial->setBaudRate(m_baudCombo->currentText().toInt());
    m_serial->setDataBits(m_dataBits);
    m_serial->setParity(m_parity);
    m_serial->setStopBits(m_stopBits);
    m_serial->setFlowControl(m_flowControl);

    if (m_serial->open(QIODevice::ReadWrite)) {
        m_openBtn->setText(tr("断开"));
        setStateLabel(tr("已连接"), QColor(kColorSuccess));
        setConfigEnabled(false);
        emit connectionChanged(true);
    } else {
        setStateLabel(tr("连接失败"), QColor(kColorError));
        LogPanel::error(tr("串口 %1 打开失败：%2")
                        .arg(portName, m_serial->errorString()));
    }
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
    m_stateLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(color.name()));
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
    if (error == QSerialPort::ResourceError && m_serial->isOpen()) {
        m_serial->close();
        m_openBtn->setText(tr("连接"));
        setStateLabel(tr("连接断开"), QColor(kColorError));
        setConfigEnabled(true);
        emit connectionChanged(false);
        LogPanel::error(tr("串口异常断开：%1").arg(m_serial->errorString()));
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
    dataBitsCombo->setCurrentIndex(
        dataBitsCombo->findData(int(m_dataBits)));

    auto *parityCombo = new QComboBox(&dlg);
    parityCombo->addItem(tr("无"),      int(QSerialPort::NoParity));
    parityCombo->addItem(tr("偶校验"),  int(QSerialPort::EvenParity));
    parityCombo->addItem(tr("奇校验"),  int(QSerialPort::OddParity));
    parityCombo->addItem(tr("标记"),    int(QSerialPort::MarkParity));
    parityCombo->addItem(tr("空格"),    int(QSerialPort::SpaceParity));
    parityCombo->setCurrentIndex(
        parityCombo->findData(int(m_parity)));

    auto *stopBitsCombo = new QComboBox(&dlg);
    stopBitsCombo->addItem(QStringLiteral("1"),   int(QSerialPort::OneStop));
    stopBitsCombo->addItem(QStringLiteral("1.5"), int(QSerialPort::OneAndHalfStop));
    stopBitsCombo->addItem(QStringLiteral("2"),   int(QSerialPort::TwoStop));
    stopBitsCombo->setCurrentIndex(
        stopBitsCombo->findData(int(m_stopBits)));

    auto *flowCombo = new QComboBox(&dlg);
    flowCombo->addItem(tr("无"),               int(QSerialPort::NoFlowControl));
    flowCombo->addItem(tr("硬件 (RTS/CTS)"),   int(QSerialPort::HardwareControl));
    flowCombo->addItem(tr("软件 (XON/XOFF)"),  int(QSerialPort::SoftwareControl));
    flowCombo->setCurrentIndex(
        flowCombo->findData(int(m_flowControl)));

    form->addRow(makeFormLabel(tr("数据位"), &dlg), dataBitsCombo);
    form->addRow(makeFormLabel(tr("校验位"), &dlg), parityCombo);
    form->addRow(makeFormLabel(tr("停止位"), &dlg), stopBitsCombo);
    form->addRow(makeFormLabel(tr("流控"),   &dlg), flowCombo);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;

    m_dataBits    = static_cast<QSerialPort::DataBits>(
                        dataBitsCombo->currentData().toInt());
    m_parity      = static_cast<QSerialPort::Parity>(
                        parityCombo->currentData().toInt());
    m_stopBits    = static_cast<QSerialPort::StopBits>(
                        stopBitsCombo->currentData().toInt());
    m_flowControl = static_cast<QSerialPort::FlowControl>(
                        flowCombo->currentData().toInt());

    updateAdvancedButtonText();
}

void SerialPanel::updateAdvancedButtonText()
{
    m_advancedBtn->setText(
        tr("高级选项 (%1)").arg(portSettingsSummary(m_dataBits, m_parity,
                                                    m_stopBits, m_flowControl)));
}
