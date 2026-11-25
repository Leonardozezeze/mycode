#include "canpanel.h"
#include "canframe.h"
#include "logpanel.h"

#include <QCanBus>
#include <QCanBusDevice>
#include <QCanBusFrame>
#include <QFile>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_LINUX
#include <net/if.h>
#include <net/if_arp.h>

namespace
{
bool isInterfaceName(const QByteArray &name)
{
    return !name.isEmpty() && name.size() < IFNAMSIZ && name != "." && name != ".." &&
           !name.contains('/') && !name.contains('\0');
}

QByteArray readInterfaceAttribute(const QString &name, const QString &attribute)
{
    QFile file(QStringLiteral("/sys/class/net/%1/%2").arg(name, attribute));
    return file.open(QIODevice::ReadOnly) ? file.readLine().trimmed() : QByteArray();
}
} // namespace
#endif

namespace
{
// 软件过滤没有内核 CAN_RAW_FILTER 的数量限制，沿用旧上限保持界面行为一致。
constexpr int maxReceiveIdFilters = 512;

QString canPluginName()
{
#ifdef Q_OS_LINUX
    return QStringLiteral("socketcan");
#else
    // Windows 加载 PCANBasic.dll，macOS 加载 MacCAN 的 libPCBUSB.dylib。
    return QStringLiteral("peakcan");
#endif
}

QString formatBitrate(int bitrate)
{
    return bitrate > 0 ? QObject::tr("%1 kbps").arg(bitrate / 1000.0, 0, 'g', 7)
                       : QStringLiteral("--");
}
} // namespace

CanPanel::CanPanel(QWidget *parent, Role role)
    : QWidget(parent), m_role(role),
      m_settingsGroup(role == Role::Sensor ? QStringLiteral("can") : QStringLiteral("console/can"))
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(5);
    const auto formLabel = [](const QString &text, QWidget *parent)
    {
        auto *label = new QLabel(text, parent);
        label->setObjectName("formLabel");
        return label;
    };

#ifdef Q_OS_LINUX
    const QString defaultInterface = QStringLiteral("can0");
#else
    const QString defaultInterface = QStringLiteral("usb0");
#endif
    m_interface = new QLineEdit(settings.value("interface", defaultInterface).toString(), this);
    m_interface->setObjectName("canInterface");
#ifdef Q_OS_LINUX
    m_interface->setToolTip(tr("使用已启用的 SocketCAN 接口，波特率由系统配置。"));
#else
    m_interface->setToolTip(tr("PCAN 通道名：usb0 对应 PCAN_USBBUS1，usb1 对应 PCAN_USBBUS2，依此类推。"));
#endif
    QString savedIds;
    if (m_role == Role::Sensor)
        savedIds = settings.value("id", "0x123").toString();
    else
    {
        // 旧版控制台的单个 ID 可能按十进制保存，迁移时保留实际数值。
        const auto previousId = parseStandardCanId(settings.value("id").toString());
        const auto legacy = previousId ? QString::number(*previousId, 16).toUpper() : QString();
        savedIds = settings.value("receiveIds", legacy).toString();
    }
    m_canId = new QLineEdit(savedIds, this);
    m_canId->setObjectName("canId");
    if (m_role == Role::Console)
    {
        m_canId->setPlaceholderText(tr("如 651, 652, 7FF"));
        m_canId->setToolTip(tr("标准帧 ID 为 11 位，以十六进制填写 000～7FF，可带 0x。"
                             "多个 ID 用逗号或空格分隔，留空接收所有标准数据帧。"));
    }
    else
        m_canId->setToolTip(tr("11 位标准帧 ID。0x 开头表示十六进制，其余按十进制读取。"));
    auto *form = new QFormLayout;
    form->addRow(formLabel(tr("接口"), this), m_interface);
    form->addRow(formLabel(tr("接收 ID"), this), m_canId);
#ifndef Q_OS_LINUX
    m_bitrateInput = new QLineEdit(settings.value("bitrate", "500000").toString(), this);
    m_bitrateInput->setObjectName("canBitrateInput");
    m_bitrateInput->setToolTip(tr("PCAN 通道波特率，须与总线一致，连接时生效。"));
    form->addRow(formLabel(tr("波特率"), this), m_bitrateInput);
#endif
    layout->addLayout(form);
    if (m_role == Role::Console)
    {
        m_receiveIdStatus = new QLabel(this);
        m_receiveIdStatus->setObjectName("canReceiveIdStatus");
        m_receiveIdStatus->setWordWrap(true);
        layout->addWidget(m_receiveIdStatus);
    }

    m_connect = new QPushButton(tr("连接"), this);
    m_connect->setObjectName("primaryButton");
    m_connect->setMinimumHeight(28);
    m_connect->setMinimumWidth(84);
    m_status = new QLabel(tr("未连接"), this);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    auto *connectionRow = new QHBoxLayout;
    connectionRow->addWidget(m_status, 1);
    connectionRow->addWidget(m_connect);
    layout->addLayout(connectionRow);

    auto *diagnostics = new QFrame(this);
    diagnostics->setObjectName("canDiagnostics");
    diagnostics->setStyleSheet(QStringLiteral(
        "QFrame#canDiagnostics { background: #252525; border: 1px solid #3a3a3a; border-radius: 6px; }"));
    auto *readouts = new QGridLayout(diagnostics);
    readouts->setContentsMargins(6, 6, 6, 6);
    readouts->setHorizontalSpacing(5);
    readouts->setVerticalSpacing(4);
    readouts->setColumnStretch(1, 1);
    readouts->setColumnStretch(3, 1);
    const auto addReadout = [diagnostics, readouts, formLabel](const QString &label, const char *name,
                                                             int row, int column)
    {
        auto *value = new QLabel(QStringLiteral("--"), diagnostics);
        value->setObjectName(QLatin1String(name));
        readouts->addWidget(formLabel(label, diagnostics), row, column);
        readouts->addWidget(value, row, column + 1);
        return value;
    };
    m_interfaceState = addReadout(tr("接口"), "canInterfaceState", 0, 0);
#ifdef Q_OS_LINUX
    m_interfaceState->setToolTip(tr("系统接口需先启用，程序才能连接。"));
#else
    m_interfaceState->setToolTip(tr("PCAN 通道连接状态。"));
#endif
    m_bitrate = addReadout(tr("波特率"), "canBitrate", 0, 2);
#ifdef Q_OS_LINUX
    m_bitrate->setToolTip(tr("通过系统的 ip 命令读取，应与发送端一致。"));
#else
    m_bitrate->setToolTip(tr("连接时写入 PCAN 通道，应与发送端一致。"));
#endif
    m_traffic = addReadout(tr("接收"), "canTraffic", 1, 0);
    m_traffic->setWordWrap(true);
    m_traffic->setToolTip(tr("统计经过接收过滤的数据帧及有效载荷字节，每次连接时清零。"));
    m_lastReceived = addReadout(tr("最近"), "canLastReceive", 1, 2);
    layout->addWidget(diagnostics);

    auto *frameFormat = new QLabel(m_role == Role::Sensor
        ? tr("DLC 8 · 0～3 含杂率 · 4～7 破碎率")
        : tr("命令号 1 字节 · 参数 0～7 字节"), this);
    frameFormat->setObjectName("deviceDesc");
    frameFormat->setWordWrap(true);
    if (m_role == Role::Sensor)
        frameFormat->setToolTip(tr("11 位标准帧。字节 0～3 是含杂率，4～7 是破碎率。"
                                   "两个字段都是小端 float32，传输 0～1 的比例。"));
    layout->addWidget(frameFormat);
    layout->addStretch();

    connect(m_connect, &QPushButton::clicked, this, &CanPanel::toggleConnection);
    if (m_receiveIdStatus)
    {
        connect(m_canId, &QLineEdit::textChanged, this, &CanPanel::refreshReceiveIds);
        refreshReceiveIds();
    }
    auto *trafficTimer = new QTimer(this);
    trafficTimer->setSingleShot(true);
    trafficTimer->setInterval(250);
    connect(trafficTimer, &QTimer::timeout, this, &CanPanel::refreshTraffic);
    connect(this, &CanPanel::frameReceived, this, [this, trafficTimer](quint32, const QByteArray &payload)
    {
        ++m_receivedFrames;
        m_receivedBytes += quint64(payload.size());
        m_lastReceivedAt = QTime::currentTime();
        if (!trafficTimer->isActive())
            trafficTimer->start();
    });
    connect(this, &CanPanel::connectionChanged, this, [this](bool connected)
    {
        if (connected)
        {
            m_receivedFrames = m_receivedBytes = 0;
            m_lastReceivedAt = {};
        }
        refreshTraffic();
    });
    refreshTraffic();

#ifdef Q_OS_LINUX
    m_linkQuery = new QProcess(this);
    m_linkQuery->setProgram(QStandardPaths::findExecutable(QStringLiteral("ip")));
    connect(m_linkQuery, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status)
    {
        const auto output = m_linkQuery->readAllStandardOutput();
        if (m_queriedInterface != m_interface->text().trimmed())
            return;
        int bitrate = 0;
        if (code == 0 && status == QProcess::NormalExit)
        {
            const auto links = QJsonDocument::fromJson(output).array();
            if (!links.isEmpty())
            {
                const auto details = links.first().toObject().value("linkinfo").toObject()
                                         .value("info_data").toObject();
                bitrate = details.value("bittiming").toObject().value("bitrate")
                              .toInt(details.value("bittiming_bitrate").toInt());
            }
        }
        m_bitrate->setText(formatBitrate(bitrate));
    });
    connect(m_linkQuery, &QProcess::errorOccurred, this, [this] { m_bitrate->setText(QStringLiteral("--")); });
    auto *interfaceTimer = new QTimer(this);
    interfaceTimer->setInterval(1500);
    connect(interfaceTimer, &QTimer::timeout, this, [this]
    {
        if (isVisible())
            refreshInterface();
    });
    interfaceTimer->start();
    connect(m_interface, &QLineEdit::textChanged, this, [this]
    {
        m_bitrate->setText(QStringLiteral("--"));
        refreshInterface();
    });
#else
    connect(m_bitrateInput, &QLineEdit::textChanged, this, [this] { refreshInterface(); });
    refreshInterface();
#endif
}

CanPanel::~CanPanel()
{
    closeDevice();
    if (m_linkQuery && m_linkQuery->state() != QProcess::NotRunning)
    {
        m_linkQuery->disconnect(this);
        m_linkQuery->kill();
        m_linkQuery->waitForFinished();
    }
}

void CanPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    refreshInterface();
}

void CanPanel::refreshInterface()
{
#ifdef Q_OS_LINUX
    const auto name = m_interface->text().trimmed();
    QString state;
    if (!isInterfaceName(QFile::encodeName(name)))
        state = tr("接口名无效");
    else
    {
        const auto type = readInterfaceAttribute(name, QStringLiteral("type"));
        if (type.isEmpty())
            state = tr("未找到接口");
        else if (type.toInt() != ARPHRD_CAN)
            state = tr("非 CAN 接口");
        else
        {
            bool ok = false;
            const uint flags = readInterfaceAttribute(name, QStringLiteral("flags")).toUInt(&ok, 0);
            state = !ok ? tr("无法读取") : (flags & IFF_UP) ? tr("已启用") : tr("未启用");
            m_interfaceState->setText(state);
            if (!m_linkQuery->program().isEmpty() && m_linkQuery->state() == QProcess::NotRunning)
            {
                m_queriedInterface = name;
                m_linkQuery->setArguments({"-details", "-json", "link", "show", "dev", name});
                m_linkQuery->start();
            }
            return;
        }
    }
    m_interfaceState->setText(state);
    m_bitrate->setText(QStringLiteral("--"));
#else
    m_interfaceState->setText(m_connected ? tr("已连接")
                              : m_device ? tr("连接中") : tr("未连接"));
    bool ok = false;
    const int bitrate = m_bitrateInput->text().trimmed().toInt(&ok);
    m_bitrate->setText(ok ? formatBitrate(bitrate) : QStringLiteral("--"));
#endif
}

void CanPanel::refreshTraffic()
{
    const QLocale locale;
    m_traffic->setText(tr("%1 帧 · %2 B").arg(locale.toString(m_receivedFrames),
                                            locale.toString(m_receivedBytes)));
    m_lastReceived->setText(m_lastReceivedAt.isValid()
        ? m_lastReceivedAt.toString(QStringLiteral("HH:mm:ss.zzz")) : QStringLiteral("--"));
}

QList<quint32> CanPanel::receiveIds(QString &error) const
{
    const auto text = m_canId->text().trimmed();
    if (m_role == Role::Sensor)
    {
        if (const auto id = parseStandardCanId(text))
            return {*id};
        error = tr("请输入 0～2047 或 0x000～0x7FF 的标准帧 ID。");
        return {};
    }
    if (text.isEmpty())
        return {};

    static const QRegularExpression separators(QStringLiteral("[,，\\s]+"));
    const auto entries = text.split(separators, Qt::SkipEmptyParts);
    if (entries.isEmpty())
    {
        error = tr("请填写十六进制 ID，或清空以接收所有标准数据帧。");
        return {};
    }

    QList<quint32> ids;
    for (const auto &entry : entries)
    {
        const auto id = parseStandardHexCanId(entry);
        if (!id)
        {
            error = tr("接收 ID 须在十六进制 000～7FF 范围内，例如 651, 652。");
            return {};
        }
        if (!ids.contains(*id))
            ids.append(*id);
    }
    if (ids.size() > maxReceiveIdFilters)
    {
        error = tr("最多可筛选 %1 个不同的 ID。").arg(maxReceiveIdFilters);
        return {};
    }
    return ids;
}

void CanPanel::refreshReceiveIds()
{
    QString error;
    const auto ids = receiveIds(error);
    m_connect->setEnabled(m_connected || error.isEmpty());
    if (!error.isEmpty())
        m_receiveIdStatus->setText(error);
    else if (ids.isEmpty())
        m_receiveIdStatus->setText(tr("接收所有标准数据帧"));
    else
        m_receiveIdStatus->setText(tr("筛选 %1 个接收 ID").arg(ids.size()));

    const auto style = error.isEmpty() ? QStringLiteral("color: #8a8a8a;")
                                       : QStringLiteral("color: #ffb347;");
    if (m_receiveIdStatus->styleSheet() != style)
        m_receiveIdStatus->setStyleSheet(style);
}

void CanPanel::toggleConnection()
{
    if (m_device)
    {
        disconnectDevice();
        return;
    }

    QString error;
    const auto ids = receiveIds(error);
    if (!error.isEmpty())
    {
        showError(error);
        return;
    }
    const auto name = m_interface->text().trimmed();
    bool ok = false;
#ifdef Q_OS_LINUX
    if (!isInterfaceName(QFile::encodeName(name)))
    {
        showError(tr("请输入有效的 SocketCAN 接口名，例如 can0。"));
        return;
    }
    const auto type = readInterfaceAttribute(name, QStringLiteral("type"));
    if (type.isEmpty())
    {
        showError(tr("未找到接口 %1。").arg(name));
        return;
    }
    if (type.toInt() != ARPHRD_CAN)
    {
        showError(tr("%1 不是可用的 SocketCAN 接口。").arg(name));
        return;
    }
    const uint flags = readInterfaceAttribute(name, QStringLiteral("flags")).toUInt(&ok, 0);
    if (!ok || !(flags & IFF_UP))
    {
        showError(tr("请先在系统中启用 CAN 接口 %1。").arg(name));
        return;
    }
#else
    static const QRegularExpression channelName(QStringLiteral("\\Ausb(?:[0-9]|1[0-5])\\z"));
    if (!channelName.match(name).hasMatch())
    {
        showError(tr("请输入 PCAN 通道名 usb0～usb15，usb0 对应 PCAN_USBBUS1。"));
        return;
    }
    const int bitrate = m_bitrateInput->text().trimmed().toInt(&ok);
    if (!ok || bitrate < 1000 || bitrate > 1000000)
    {
        showError(tr("波特率须为 1000～1000000 之间的整数，例如 500000。"));
        return;
    }
#endif

    m_device = QCanBus::instance()->createDevice(canPluginName(), name, &error);
    if (!m_device)
    {
        showError(tr("无法创建 CAN 设备：%1").arg(error));
        return;
    }
#ifndef Q_OS_LINUX
    m_device->setConfigurationParameter(QCanBusDevice::BitRateKey, bitrate);
#endif
    m_filterIds = ids;
    connect(m_device, &QCanBusDevice::framesReceived, this, &CanPanel::readFrames);
    connect(m_device, &QCanBusDevice::stateChanged, this,
            [this](QCanBusDevice::CanBusDeviceState state)
    {
        if (!m_device)
            return;
        if (state == QCanBusDevice::ConnectedState)
            handleConnected();
        else if (state == QCanBusDevice::UnconnectedState)
            handleUnconnected();
    });
    connect(m_device, &QCanBusDevice::errorOccurred, this, [this]
    {
        if (m_device && m_connected)
            showError(tr("CAN 错误：%1").arg(m_device->errorString()));
    });
    m_interface->setEnabled(false);
    m_canId->setEnabled(false);
#ifndef Q_OS_LINUX
    m_bitrateInput->setEnabled(false);
#endif
    m_connect->setText(tr("断开"));
    m_status->setText(tr("连接中…"));
    if (!m_device->connectDevice())
    {
        showError(tr("无法连接 CAN：%1").arg(m_device->errorString()));
        handleUnconnected();
    }
}

void CanPanel::handleConnected()
{
    m_connected = true;
    const auto name = m_interface->text().trimmed();
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue("interface", name);
    settings.setValue(m_role == Role::Sensor ? "id" : "receiveIds", m_canId->text().trimmed());
#ifndef Q_OS_LINUX
    settings.setValue("bitrate", m_bitrateInput->text().trimmed());
#endif
    m_status->setText(tr("已连接 · %1").arg(name));
    refreshInterface();
    emit connectionChanged(true);
}

void CanPanel::handleUnconnected()
{
    const bool wasConnected = m_connected;
    m_connected = false;
    closeDevice();
    m_interface->setEnabled(true);
    m_canId->setEnabled(true);
#ifndef Q_OS_LINUX
    m_bitrateInput->setEnabled(true);
#endif
    m_connect->setText(tr("连接"));
    if (m_receiveIdStatus)
        refreshReceiveIds();
    m_status->setText(tr("未连接"));
    refreshInterface();
    if (wasConnected)
        emit connectionChanged(false);
}

QString CanPanel::sendCommand(quint32 id, const QByteArray &payload)
{
    if (m_role != Role::Console || !m_connected || !m_device)
        return tr("控制台 CAN 未连接");
    if (id > maxStandardCanId)
        return tr("发送 ID 超出 11 位标准帧范围");
    if (payload.isEmpty() || payload.size() > maxClassicCanPayload)
        return tr("CAN 命令需要 1 字节命令号和最多 7 字节参数");

    QCanBusFrame frame(id, payload);
    frame.setExtendedFrameFormat(false);
    if (!m_device->writeFrame(frame))
    {
        const auto error = m_device->errorString();
        return error.isEmpty() ? tr("CAN 帧发送失败") : error;
    }
    return {};
}

void CanPanel::readFrames()
{
    // 每次最多处理一批帧，让持续收包时界面也能处理事件。
    for (int i = 0; i < 256 && m_device && m_device->framesAvailable() > 0; ++i)
    {
        const QCanBusFrame frame = m_device->readFrame();
        // 与旧内核过滤器一致：只收标准数据帧，排除扩展帧和远程帧。
        if (frame.frameType() != QCanBusFrame::DataFrame || frame.hasExtendedFrameFormat())
            continue;
        const quint32 id = frame.frameId() & maxStandardCanId;
        if (!m_filterIds.isEmpty() && !m_filterIds.contains(id))
            continue;
        emit frameReceived(id, frame.payload());
    }
}

void CanPanel::closeDevice()
{
    if (!m_device)
        return;
    m_device->disconnect(this);
    if (m_device->state() != QCanBusDevice::UnconnectedState)
        m_device->disconnectDevice();
    m_device->deleteLater();
    m_device = nullptr;
}

void CanPanel::disconnectDevice()
{
    if (!m_device)
        return;
    m_device->disconnectDevice();
    // 插件若未同步发出 stateChanged，这里兜底完成清理。
    if (m_device)
        handleUnconnected();
}

void CanPanel::showError(const QString &message)
{
    m_status->setText(message);
    LogPanel::error(message);
}
