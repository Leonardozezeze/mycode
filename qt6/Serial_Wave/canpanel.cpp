#include "canpanel.h"
#include "canframe.h"
#include "logpanel.h"

#include <QFile>
#include <QFormLayout>
#include <QFrame>
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
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_LINUX
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace
{
QString socketError()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}

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

CanPanel::CanPanel(QWidget *parent, Role role)
    : QWidget(parent), m_role(role),
      m_settingsGroup(role == Role::Sensor ? QStringLiteral("can") : QStringLiteral("console/can"))
{
    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);
    auto *title = new QLabel(tr("CAN 配置"), this);
    title->setObjectName("panelTitle");
    layout->addWidget(title);
    const auto formLabel = [](const QString &text, QWidget *parent)
    {
        auto *label = new QLabel(text, parent);
        label->setObjectName("formLabel");
        return label;
    };

    m_interface = new QLineEdit(settings.value("interface", "can0").toString(), this);
    m_interface->setObjectName("canInterface");
    m_interface->setToolTip(tr("使用已启用的 SocketCAN 接口，波特率由系统配置。"));
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
    layout->addWidget(m_connect);
    m_status = new QLabel(tr("未连接"), this);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);

    auto *diagnostics = new QFrame(this);
    diagnostics->setObjectName("canDiagnostics");
    diagnostics->setStyleSheet(QStringLiteral(
        "QFrame#canDiagnostics { background: #252525; border: 1px solid #3a3a3a; border-radius: 6px; }"));
    auto *readouts = new QFormLayout(diagnostics);
    readouts->setContentsMargins(10, 8, 10, 8);
    readouts->setHorizontalSpacing(12);
    readouts->setVerticalSpacing(4);
    const auto addReadout = [diagnostics, readouts, formLabel](const QString &label, const char *name)
    {
        auto *value = new QLabel(QStringLiteral("--"), diagnostics);
        value->setObjectName(QLatin1String(name));
        readouts->addRow(formLabel(label, diagnostics), value);
        return value;
    };
    m_interfaceState = addReadout(tr("接口状态"), "canInterfaceState");
    m_interfaceState->setToolTip(tr("系统接口需先启用，程序才能连接。"));
    m_bitrate = addReadout(tr("系统波特率"), "canBitrate");
    m_bitrate->setToolTip(tr("通过系统的 ip 命令读取，应与发送端一致。"));
    m_traffic = addReadout(tr("接收流量"), "canTraffic");
    m_traffic->setToolTip(tr("统计经过接收过滤的数据帧及有效载荷字节，每次连接时清零。"));
    m_lastReceived = addReadout(tr("最近接收"), "canLastReceive");
    layout->addWidget(diagnostics);

    auto *frameFormat = new QLabel(m_role == Role::Sensor
        ? tr("11 位标准帧 · DLC 8\n0～3 字节 含杂率 · 4～7 字节 破碎率")
        : tr("发送 ID 0x650 · 命令号 1 字节 · 参数 0～7 字节"), this);
    frameFormat->setObjectName("deviceDesc");
    frameFormat->setWordWrap(true);
    if (m_role == Role::Sensor)
        frameFormat->setToolTip(tr("两个字段都是小端 float32，传输 0～1 的比例。"));
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
        m_bitrate->setText(bitrate > 0 ? tr("%1 kbps").arg(bitrate / 1000.0, 0, 'g', 7)
                                       : QStringLiteral("--"));
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
#endif
}

CanPanel::~CanPanel()
{
    closeSocket();
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
    static const QRegularExpression hexId(QStringLiteral("\\A(?:0[xX])?[0-9A-Fa-f]+\\z"));
    const auto entries = text.split(separators, Qt::SkipEmptyParts);
    if (entries.isEmpty())
    {
        error = tr("请填写十六进制 ID，或清空以接收所有标准数据帧。");
        return {};
    }

    QList<quint32> ids;
    for (const auto &entry : entries)
    {
        bool ok = false;
        const auto id = entry.toUInt(&ok, 16);
        if (!ok || id > maxStandardCanId || !hexId.match(entry).hasMatch())
        {
            error = tr("接收 ID 须在十六进制 000～7FF 范围内，例如 651, 652。");
            return {};
        }
        if (!ids.contains(id))
            ids.append(id);
    }
#ifdef Q_OS_LINUX
    if (ids.size() > CAN_RAW_FILTER_MAX)
    {
        error = tr("最多可筛选 %1 个不同的 ID。").arg(CAN_RAW_FILTER_MAX);
        return {};
    }
#endif
    return ids;
}

void CanPanel::refreshReceiveIds()
{
    QString error;
    const auto ids = receiveIds(error);
    m_connect->setEnabled(m_socket >= 0 || error.isEmpty());
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
    if (m_socket >= 0)
    {
        disconnectDevice();
        return;
    }

#ifdef Q_OS_LINUX
    const auto idText = m_canId->text().trimmed();
    QString error;
    const auto ids = receiveIds(error);
    if (!error.isEmpty())
    {
        showError(error);
        return;
    }
    const auto name = m_interface->text().trimmed();
    const auto device = QFile::encodeName(name);
    if (!isInterfaceName(device))
    {
        showError(tr("请输入有效的 SocketCAN 接口名，例如 can0。"));
        return;
    }

    m_socket = ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);
    if (m_socket < 0)
    {
        showError(tr("无法打开 SocketCAN：%1").arg(socketError()));
        return;
    }
    const auto fail = [this](const QString &message)
    {
        closeSocket();
        showError(message);
    };
    ifreq request{};
    std::memcpy(request.ifr_name, device.constData(), device.size());
    if (::ioctl(m_socket, SIOCGIFINDEX, &request) < 0)
    {
        fail(tr("无法查找接口 %1：%2").arg(name, socketError()));
        return;
    }
    const int index = request.ifr_ifindex;
    if (::ioctl(m_socket, SIOCGIFHWADDR, &request) < 0 || request.ifr_hwaddr.sa_family != ARPHRD_CAN)
    {
        fail(tr("%1 不是可用的 SocketCAN 接口。").arg(name));
        return;
    }
    if (::ioctl(m_socket, SIOCGIFFLAGS, &request) < 0 || !(request.ifr_flags & IFF_UP))
    {
        fail(tr("请先在系统中启用 CAN 接口 %1。").arg(name));
        return;
    }

    QList<can_filter> filters;
    constexpr canid_t standardDataFlags = CAN_EFF_FLAG | CAN_RTR_FLAG;
    if (ids.isEmpty())
        filters.append(can_filter{0, standardDataFlags});
    else
    {
        filters.reserve(ids.size());
        for (const auto id : ids)
            filters.append(can_filter{id, standardDataFlags | CAN_SFF_MASK});
    }
    const auto filterBytes = socklen_t(filters.size() * sizeof(can_filter));
    if (::setsockopt(m_socket, SOL_CAN_RAW, CAN_RAW_FILTER, filters.constData(), filterBytes) < 0)
    {
        fail(tr("无法设置 CAN 报文过滤器：%1").arg(socketError()));
        return;
    }
    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = index;
    if (::bind(m_socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) < 0)
    {
        fail(tr("无法连接 CAN 接口 %1：%2").arg(name, socketError()));
        return;
    }

    QSettings settings;
    settings.beginGroup(m_settingsGroup);
    settings.setValue("interface", name);
    settings.setValue(m_role == Role::Sensor ? "id" : "receiveIds", idText);
    m_interface->setEnabled(false);
    m_canId->setEnabled(false);
    m_connect->setText(tr("断开"));
    m_status->setText(tr("已连接 · %1").arg(name));
    m_notifier = new QSocketNotifier(m_socket, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &CanPanel::readFrames);
    emit connectionChanged(true);
#else
    showError(tr("CAN 接收需要 Linux SocketCAN。"));
#endif
}

QString CanPanel::sendCommand(const QByteArray &payload)
{
#ifdef Q_OS_LINUX
    if (m_role != Role::Console || m_socket < 0)
        return tr("控制台 CAN 未连接");
    if (payload.isEmpty() || payload.size() > maxClassicCanPayload)
        return tr("CAN 命令需要 1 字节命令号和最多 7 字节参数");

    can_frame frame{};
    frame.can_id = canCommandId;
    frame.len = quint8(payload.size());
    std::memcpy(frame.data, payload.constData(), frame.len);
    ssize_t written;
    do
    {
        written = ::write(m_socket, &frame, sizeof(frame));
    }
    while (written < 0 && errno == EINTR);
    if (written < 0)
        return socketError();
    if (written != ssize_t(sizeof(frame)))
        return tr("CAN 帧未完整排队");
    return {};
#else
    Q_UNUSED(payload);
    return tr("CAN 发送需要 Linux SocketCAN");
#endif
}

void CanPanel::readFrames()
{
#ifdef Q_OS_LINUX
    // 每次最多处理一批帧，让持续收包时界面也能处理事件。
    for (int i = 0; i < 256 && m_socket >= 0; ++i)
    {
        can_frame frame{};
        const auto received = ::recv(m_socket, &frame, sizeof(frame), 0);
        if (received == sizeof(frame))
        {
            if (frame.len <= CAN_MAX_DLEN)
                emit frameReceived(frame.can_id & CAN_SFF_MASK,
                                   QByteArray(reinterpret_cast<const char *>(frame.data), frame.len));
        }
        else if (received < 0)
        {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            const auto error = socketError();
            disconnectDevice();
            showError(tr("CAN 接口读取失败：%1").arg(error));
            return;
        }
    }
#endif
}

void CanPanel::closeSocket()
{
#ifdef Q_OS_LINUX
    if (m_notifier)
    {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_socket >= 0)
    {
        ::close(m_socket);
        m_socket = -1;
    }
#endif
}

void CanPanel::disconnectDevice()
{
    closeSocket();
    m_interface->setEnabled(true);
    m_canId->setEnabled(true);
    m_connect->setText(tr("连接"));
    if (m_receiveIdStatus)
        refreshReceiveIds();
    m_status->setText(tr("未连接"));
    emit connectionChanged(false);
}

void CanPanel::showError(const QString &message)
{
    m_status->setText(message);
    LogPanel::error(message);
}
