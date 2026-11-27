#include "shellcommanddialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtEndian>

namespace
{
constexpr qsizetype maxReportBytes = 64 * 1024;
constexpr int replyTimeoutMs = 5000;
constexpr char shellInfoOpcode = '\x05';
constexpr int canHeaderBytes = 4;
}

ShellCommandDialog::ShellCommandDialog(QWidget *parent) : QDialog(parent)
{
    setObjectName("shellCommandDialog");
    setWindowTitle(tr("下位机命令列表"));
    resize(720, 440);
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    m_status = new QLabel(this);
    m_status->setObjectName("shellCommandStatus");
    m_status->setWordWrap(true);
    header->addWidget(m_status, 1);
    m_transport = new QComboBox(this);
    m_transport->setObjectName("shellCommandTransport");
    m_transport->setAccessibleName(tr("查询通道"));
    m_transport->addItems({tr("控制台 UART"), tr("控制台 CAN")});
    header->addWidget(m_transport);
    layout->addLayout(header);

    m_table = new QTableWidget(0, 3, this);
    m_table->setObjectName("shellCommandTable");
    m_table->setHorizontalHeaderLabels({tr("命令"), tr("CAN 操作码"), tr("用法")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setWordWrap(true);
    m_table->verticalHeader()->hide();
    auto *columns = m_table->horizontalHeader();
    columns->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    columns->setMinimumSectionSize(96);
    columns->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    columns->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    columns->setSectionResizeMode(2, QHeaderView::Stretch);
    connect(columns, &QHeaderView::sectionResized, m_table, &QTableWidget::resizeRowsToContents);
    layout->addWidget(m_table, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("关闭"));
    m_refresh = buttons->addButton(tr("重新获取"), QDialogButtonBox::ActionRole);
    m_refresh->setEnabled(false);
    connect(m_refresh, &QPushButton::clicked, this, &ShellCommandDialog::query);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this]
    {
        finish(selectedTransport() == Transport::Can
            ? tr("等待 CAN shell-info 第 %1 页回复超时，请检查 Shell 收发 ID。").arg(m_canPage)
            : tr("等待 shell-info 回复超时，请确认下位机支持此命令。"));
    });
    connect(m_transport, &QComboBox::currentIndexChanged, this, &ShellCommandDialog::updateSelection);
    updateSelection();
}

ShellCommandDialog::Transport ShellCommandDialog::selectedTransport() const
{
    return m_transport->currentIndex() == 0 ? Transport::Uart : Transport::Can;
}

bool ShellCommandDialog::isConnected(Transport transport) const
{
    return transport == Transport::Uart ? m_uartConnected : m_canConnected;
}

void ShellCommandDialog::setConnected(Transport transport, bool connected)
{
    (transport == Transport::Uart ? m_uartConnected : m_canConnected) = connected;
    if (transport == selectedTransport() && !m_timeout->isActive())
        updateSelection();
    else if (transport == selectedTransport() && !connected)
        finish(tr("控制台连接已断开，查询已停止。"));
}

void ShellCommandDialog::updateSelection()
{
    const bool connected = isConnected(selectedTransport());
    m_table->setRowCount(0);
    m_refresh->setEnabled(connected);
    m_status->setText(connected ? tr("可获取当前设备的命令列表。")
                                : tr("请先连接%1").arg(m_transport->currentText()));
}

void ShellCommandDialog::queryPreferred(Transport preferred)
{
    if (m_timeout->isActive())
        return;
    const auto alternative = preferred == Transport::Uart ? Transport::Can : Transport::Uart;
    const auto transport = !isConnected(preferred) && isConnected(alternative) ? alternative : preferred;
    m_transport->setCurrentIndex(transport == Transport::Uart ? 0 : 1);
    query();
}

void ShellCommandDialog::query()
{
    if (!isConnected(selectedTransport()) || m_timeout->isActive())
        return;
    m_table->setRowCount(0);
    m_rows.clear();
    m_pending.clear();
    m_canReport.clear();
    m_canResponder.reset();
    m_canPage = 0;
    m_receivedBytes = 0;
    m_inReport = false;
    m_refresh->setEnabled(false);
    m_transport->setEnabled(false);
    m_status->setText(tr("正在读取 shell-info…"));
    if (selectedTransport() == Transport::Can)
        requestCanPage();
    else
    {
        m_timeout->start(replyTimeoutMs);
        emit queryRequested();
    }
}

void ShellCommandDialog::sendFailed(const QString &error)
{
    finish(error.isEmpty() ? tr("shell-info 未完整排队，请检查控制台连接。") : error);
}

void ShellCommandDialog::requestCanPage()
{
    QByteArray payload(3, '\0');
    payload[0] = shellInfoOpcode;
    qToLittleEndian(m_canPage, payload.data() + 1);
    m_timeout->start(replyTimeoutMs);
    emit canPageRequested(payload);
}

void ShellCommandDialog::receiveCan(quint32 id, const QByteArray &payload)
{
    if (!m_timeout->isActive() || selectedTransport() != Transport::Can ||
        payload.size() < 2 || payload[0] != shellInfoOpcode ||
        (m_canResponder && *m_canResponder != id))
        return;
    const auto status = uchar(payload[1]);
    if (status)
    {
        finish(tr("下位机拒绝 CAN shell-info，状态 0x%1。").arg(status, 2, 16, QLatin1Char('0')));
        return;
    }
    if (payload.size() < canHeaderBytes || payload.size() > 8 ||
        qFromLittleEndian<quint16>(payload.constData() + 2) != m_canPage)
        return;
    m_canResponder = id;
    if (payload.size() - canHeaderBytes > maxReportBytes - m_canReport.size())
    {
        finish(tr("shell-info 回复超过大小限制。"));
        return;
    }
    m_canReport.append(payload.constData() + canHeaderBytes, payload.size() - canHeaderBytes);
    if (payload.size() == 8)
    {
        ++m_canPage;
        requestCanPage();
        return;
    }
    consumeBytes(m_canReport);
    if (m_timeout->isActive())
        finish(tr("CAN shell-info 回复不完整，缺少报告结束标记。"));
}

void ShellCommandDialog::receive(const QByteArray &bytes)
{
    if (m_timeout->isActive() && selectedTransport() == Transport::Uart)
        consumeBytes(bytes);
}

void ShellCommandDialog::consumeBytes(const QByteArray &bytes)
{
    if (bytes.size() > maxReportBytes - m_receivedBytes)
    {
        finish(tr("shell-info 回复超过大小限制。"));
        return;
    }
    m_receivedBytes += bytes.size();
    const auto previousSize = m_pending.size();
    m_pending.append(bytes);
    qsizetype start = 0;
    for (qsizetype i = previousSize; i < m_pending.size(); ++i)
    {
        if (m_pending[i] != '\r' && m_pending[i] != '\n')
            continue;
        if (i > start)
            handleLine(m_pending.mid(start, i - start));
        if (!m_timeout->isActive())
            return;
        start = i + 1;
    }
    m_pending.remove(0, start);
}

void ShellCommandDialog::handleLine(const QByteArray &raw)
{
    const auto line = raw.trimmed();
    if (line == "shell-info begin")
    {
        m_inReport = true;
        m_rows.clear();
        return;
    }
    if (!m_inReport)
    {
        if (line == "unknown command")
            finish(tr("下位机回复未知命令，当前固件可能不支持 shell-info。"));
        return;
    }
    if (line == "shell-info end")
    {
        m_table->setRowCount(int(m_rows.size()));
        for (int row = 0; row < m_table->rowCount(); ++row)
            for (int col = 0; col < m_table->columnCount(); ++col)
                m_table->setItem(row, col, new QTableWidgetItem(m_rows[row][col]));
        m_table->resizeRowsToContents();
        finish(tr("已获取 %1 条命令").arg(m_table->rowCount()));
        return;
    }
    static const QRegularExpression command(QStringLiteral(
        "^command name=(\\S+) opcode=(none|0[xX][0-9A-Fa-f]{2}) help=(.*)$"));
    const auto match = command.match(QString::fromUtf8(line));
    if (match.hasMatch())
        m_rows.append({match.captured(1), match.captured(2) == QStringLiteral("none")
                       ? tr("仅 UART") : match.captured(2), match.captured(3)});
}

void ShellCommandDialog::finish(const QString &message)
{
    m_timeout->stop();
    m_pending.clear();
    m_canReport.clear();
    m_rows.clear();
    m_transport->setEnabled(true);
    m_refresh->setEnabled(isConnected(selectedTransport()));
    m_status->setText(message);
}
