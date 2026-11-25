#include "commandbar.h"
#include "canframe.h"
#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QKeyEvent>
#include <QSettings>
#include <QRegularExpression>
#include <QInputDialog>
#include <QMenu>
#include <algorithm>
#include <optional>

namespace
{
std::optional<QByteArray> parseHexPayload(QString text)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s"));
    static const QRegularExpression invalidDigit(QStringLiteral("[^0-9a-fA-F]"));
    text.remove(whitespace);
    if (text.size() % 2 || text.contains(invalidDigit))
        return std::nullopt;
    return QByteArray::fromHex(text.toLatin1());
}

std::optional<QByteArray> parseCanHexBytes(const QString &text)
{
    // 空白只能出现在完整字节之间。
    static const QRegularExpression hexBytes(QStringLiteral("\\A(?:[0-9a-fA-F]{2}\\s*)*\\z"));
    if (!hexBytes.match(text).hasMatch())
        return std::nullopt;
    return QByteArray::fromHex(text.toLatin1());
}

void addTerminators(QComboBox *combo, const QString &noEnding)
{
    combo->addItem(QStringLiteral("\\r\\n"), QByteArray("\r\n"));
    combo->addItem(QStringLiteral("\\n"), QByteArray("\n"));
    combo->addItem(QStringLiteral("\\r"), QByteArray("\r"));
    combo->addItem(noEnding, QByteArray());
}
} // namespace

CommandBar::CommandBar(QWidget *parent) : QWidget(parent)
{
    QSettings settings;
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto *options = new QHBoxLayout;
    auto *title = new QLabel(tr("发送命令"), this);
    title->setObjectName("sectionTitle");
    options->addWidget(title);
    m_format = new QComboBox(this);
    m_format->setObjectName("sendFormatCombo");
    m_format->addItems({tr("文本 UTF-8"), tr("HEX")});
    m_format->setCurrentIndex(settings.value("command/hex", false).toBool() ? 1 : 0);
    m_terminator = new QComboBox(this);
    m_terminator->setObjectName("sendTerminatorCombo");
    addTerminators(m_terminator, tr("无结尾"));
    m_terminator->setCurrentIndex(qBound(0, settings.value("command/terminator", 0).toInt(), 3));
    m_terminator->setToolTip(tr("仅用于文本命令；HEX 模式严格按输入字节发送，不追加换行。"));
    m_keepInput = new QCheckBox(tr("发送后保留"), this);
    m_keepInput->setObjectName("keepCommandCheck");
    m_keepInput->setChecked(settings.value("command/keepInput", false).toBool());
    options->addWidget(m_format);
    options->addWidget(m_terminator);
    options->addStretch();
    options->addWidget(m_keepInput);
    layout->addLayout(options);
    auto *editor = new QHBoxLayout;
    m_input = new QLineEdit(this);
    m_input->setObjectName("commandInput");
    m_input->setMaxLength(16384);
    m_input->setClearButtonEnabled(true);
    m_input->installEventFilter(this);
    m_sendBtn = new QPushButton(tr("发送"), this);
    m_sendBtn->setObjectName("sendCommandButton");
    m_sendBtn->setToolTip(tr("回车发送；连接串口后可用"));
    editor->addWidget(m_input, 1);
    editor->addWidget(m_sendBtn);
    layout->addLayout(editor);
    auto *favorites = new QHBoxLayout;
    m_presets = new QComboBox(this);
    m_presets->setObjectName("commandPresets");
    m_presets->setMinimumContentsLength(12);
    m_presets->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    auto *save = new QPushButton(tr("收藏"), this);
    auto *remove = new QPushButton(tr("移除"), this);
    m_historyButton = new QPushButton(tr("历史"), this);
    m_historyButton->setObjectName("commandHistoryButton");
    m_historyMenu = new QMenu(m_historyButton);
    m_historyButton->setMenu(m_historyMenu);
    favorites->addWidget(m_presets, 1);
    favorites->addWidget(save);
    favorites->addWidget(remove);
    favorites->addWidget(m_historyButton);
    layout->addLayout(favorites);
    m_feedback = new QLabel(this);
    m_feedback->setObjectName("commandFeedback");
    layout->addWidget(m_feedback);
    for (const auto &item : settings.value("command/presets").toList())
    {
        const auto map = item.toMap();
        Command cmd{map.value("name").toString().left(64), map.value("text").toString().left(16384),
                    map.value("hex").toBool(), qBound(0, map.value("terminator").toInt(), 3)};
        if (!cmd.name.isEmpty() && !cmd.text.isEmpty() && m_saved.size() < 30)
            m_saved.append(cmd);
    }
    rebuildPresets();
    rebuildHistory();
    connect(m_input, &QLineEdit::returnPressed, this, &CommandBar::submit);
    connect(m_sendBtn, &QPushButton::clicked, this, &CommandBar::submit);
    connect(m_input, &QLineEdit::textChanged, this,
            [this]
            {
                if (!m_applying)
                {
                    m_historyIndex = m_history.size();
                    m_presets->setCurrentIndex(0);
                }
                updateEditor();
            });
    connect(m_format, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                QSettings().setValue("command/hex", index == 1);
                updateEditor();
            });
    connect(m_terminator, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                QSettings().setValue("command/terminator", index);
                updateEditor();
            });
    connect(m_keepInput, &QCheckBox::toggled, this,
            [](bool keep) { QSettings().setValue("command/keepInput", keep); });
    connect(m_presets, &QComboBox::currentIndexChanged, this,
            [this, remove](int index)
            {
                remove->setEnabled(index > 0);
                if (index > 0 && index <= m_saved.size())
                {
                    applyCommand(m_saved[index - 1]);
                    m_historyIndex = m_history.size();
                }
            });
    remove->setEnabled(false);
    connect(save, &QPushButton::clicked, this,
            [this]
            {
                if (m_input->text().isEmpty())
                    return;
                bool accepted = false;
                const auto name = QInputDialog::getText(
                    this, tr("收藏命令"), tr("名称（选择收藏只会填入，不会自动发送）"),
                    QLineEdit::Normal, m_input->text().left(24), &accepted);
                if (accepted)
                    savePreset(name);
            });
    connect(remove, &QPushButton::clicked, this,
            [this]
            {
                const int index = m_presets->currentIndex() - 1;
                if (index >= 0 && index < m_saved.size())
                {
                    m_saved.removeAt(index);
                    writePresets();
                    rebuildPresets();
                }
            });
    setConnected(false);
}
CommandBar::Command CommandBar::currentCommand() const
{
    return {{}, m_input->text(), isHexMode(), m_terminator->currentIndex()};
}
bool CommandBar::isHexMode() const
{
    return m_format->currentIndex() == 1;
}
QByteArray CommandBar::payload(QString *error) const
{
    if (m_input->text().isEmpty())
        return {};
    if (!isHexMode())
        return m_input->text().toUtf8() + m_terminator->currentData().toByteArray();
    const auto bytes = parseHexPayload(m_input->text());
    if (bytes)
        return *bytes;
    if (error)
        *error = tr("HEX 请输入成对十六进制字符，例如 AA 55 01；不加 0x。");
    return {};
}
void CommandBar::updateEditor()
{
    QString error;
    const auto bytes = payload(&error);
    m_terminator->setEnabled(!isHexMode());
    m_sendBtn->setEnabled(m_connected && !bytes.isEmpty() && error.isEmpty());
    m_input->setPlaceholderText(isHexMode() ? tr("例如 AA 55 01（不追加换行）")
                                            : tr("输入命令；回车发送；上下键查看历史"));
    m_feedback->setText(!error.isEmpty()
                            ? error
                            : tr("%1 字节%2")
                                  .arg(bytes.size())
                                  .arg(m_connected ? QString() : tr(" · 未连接，可先编辑和收藏")));
    const auto style =
        error.isEmpty() ? QStringLiteral("color: #8a8a8a;") : QStringLiteral("color: #ffb347;");
    if (m_feedback->styleSheet() != style)
        m_feedback->setStyleSheet(style);
}
void CommandBar::setConnected(bool connected)
{
    m_connected = connected;
    updateEditor();
}
void CommandBar::submit()
{
    QString error;
    const auto bytes = payload(&error);
    if (!m_connected || bytes.isEmpty() || !error.isEmpty())
    {
        updateEditor();
        return;
    }
    emit commandSubmitted(bytes);
}
void CommandBar::applyCommand(const Command &command)
{
    m_applying = true;
    m_format->setCurrentIndex(command.hex ? 1 : 0);
    m_terminator->setCurrentIndex(qBound(0, command.terminator, 3));
    m_input->setText(command.text);
    m_applying = false;
    updateEditor();
}
void CommandBar::acceptSubmission()
{
    const auto command = currentCommand();
    for (int i = m_history.size() - 1; i >= 0; --i)
        if (m_history[i].text == command.text && m_history[i].hex == command.hex &&
            m_history[i].terminator == command.terminator)
            m_history.removeAt(i);
    m_history.append(command);
    if (m_history.size() > 50)
        m_history.removeFirst();
    m_historyIndex = m_history.size();
    m_draft = {};
    if (!m_keepInput->isChecked())
        m_input->clear();
    rebuildHistory();
    updateEditor();
}
void CommandBar::rebuildHistory()
{
    m_historyMenu->clear();
    m_historyButton->setEnabled(!m_history.isEmpty());
    for (int i = m_history.size() - 1; i >= 0; --i)
    {
        const auto command = m_history[i];
        const auto title = (command.hex ? QStringLiteral("HEX  ") : QStringLiteral("TXT  ")) +
                           command.text.left(40);
        m_historyMenu->addAction(title, this,
                                 [this, command]
                                 {
                                     applyCommand(command);
                                     m_historyIndex = m_history.size();
                                     m_input->setFocus();
                                 });
    }
}
bool CommandBar::savePreset(const QString &name)
{
    QString error;
    const auto bytes = payload(&error);
    const auto label = name.trimmed().left(64);
    if (label.isEmpty() || bytes.isEmpty() || !error.isEmpty())
        return false;
    auto command = currentCommand();
    command.name = label;
    int index = -1;
    for (int i = 0; i < m_saved.size(); ++i)
        if (m_saved[i].name == label)
        {
            index = i;
            break;
        }
    if (index < 0)
    {
        if (m_saved.size() >= 30)
        {
            m_feedback->setText(tr("最多收藏 30 条命令，请先移除一条。"));
            return false;
        }
        m_saved.append(command);
    }
    else
        m_saved[index] = command;
    writePresets();
    rebuildPresets();
    return true;
}
void CommandBar::writePresets()
{
    QVariantList saved;
    for (const auto &c : m_saved)
    {
        QVariantMap map;
        map["name"] = c.name;
        map["text"] = c.text;
        map["hex"] = c.hex;
        map["terminator"] = c.terminator;
        saved.append(map);
    }
    QSettings().setValue("command/presets", saved);
}
void CommandBar::rebuildPresets()
{
    m_presets->clear();
    m_presets->addItem(tr("常用命令…"));
    for (const auto &c : m_saved)
        m_presets->addItem(c.name);
    m_presets->setCurrentIndex(0);
}
bool CommandBar::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_input && event->type() == QEvent::KeyPress && !m_history.isEmpty())
    {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Up || key == Qt::Key_Down)
        {
            if (m_historyIndex == m_history.size())
                m_draft = currentCommand();
            m_historyIndex =
                qBound(0, m_historyIndex + (key == Qt::Key_Up ? -1 : 1), int(m_history.size()));
            applyCommand(m_historyIndex == m_history.size() ? m_draft : m_history[m_historyIndex]);
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

CanCommandBar::CanCommandBar(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto *form = new QFormLayout;
    m_command = new QLineEdit(this);
    m_command->setObjectName("canCommandNumber");
    m_command->setPlaceholderText(tr("例如 0x01 或十进制 1"));
    auto *commandRow = new QHBoxLayout;
    commandRow->addWidget(m_command, 1);
    auto *converter = new QPushButton(tr("进制转换…"), this);
    converter->setObjectName("canNumberConverterButton");
    commandRow->addWidget(converter);
    auto *commandLabel = new QLabel(tr("命令号"), this);
    commandLabel->setBuddy(m_command);
    form->addRow(commandLabel, commandRow);

    auto *editor = new QHBoxLayout;
    m_parameters = new QLineEdit(this);
    m_parameters->setObjectName("canCommandParameters");
    m_parameters->setPlaceholderText(tr("例如 AA 55 或 AA55，可留空"));
    m_parameters->setToolTip(tr("每两位十六进制字符表示 1 字节。可在字节间加空格，不加 0x。"));
    m_parameters->setClearButtonEnabled(true);
    m_send = new QPushButton(tr("发送"), this);
    m_send->setObjectName("canSendButton");
    editor->addWidget(m_parameters, 1);
    editor->addWidget(m_send);
    auto *dataLabel = new QLabel(tr("HEX 参数"), this);
    dataLabel->setBuddy(m_parameters);
    form->addRow(dataLabel, editor);
    layout->addLayout(form);

    m_feedback = new QLabel(this);
    m_feedback->setObjectName("canSendFeedback");
    m_feedback->setWordWrap(true);
    layout->addWidget(m_feedback);
    connect(m_command, &QLineEdit::textChanged, this, &CanCommandBar::updateEditor);
    connect(m_parameters, &QLineEdit::textChanged, this, &CanCommandBar::updateEditor);
    connect(m_send, &QPushButton::clicked, this, &CanCommandBar::submit);
    connect(converter, &QPushButton::clicked, this, &CanCommandBar::openNumberConverter);
    connect(m_command, &QLineEdit::returnPressed, this, &CanCommandBar::submit);
    connect(m_parameters, &QLineEdit::returnPressed, this, &CanCommandBar::submit);
    updateEditor();
}

void CanCommandBar::setConnected(bool connected)
{
    m_connected = connected;
    updateEditor();
}

QByteArray CanCommandBar::commandPayload(QString &error, qsizetype *parameterCount) const
{
    const auto parameters = parseCanHexBytes(m_parameters->text().trimmed());
    if (!parameters)
    {
        error = tr("每个 HEX 字节须有两位，例如 0A FF；不加 0x。");
        return {};
    }

    if (parameterCount)
        *parameterCount = parameters->size();
    if (parameters->size() >= maxClassicCanPayload)
    {
        error = tr("CAN 命令最多支持 7 字节参数。");
        return {};
    }

    const auto text = m_command->text().trimmed();
    if (text.isEmpty())
        return {};
    const auto command = parseStandardCanId(text);
    if (!command || *command > 0xFF)
    {
        error = tr("命令号必须在 0～255 或 0x00～0xFF 范围内。");
        return {};
    }

    return QByteArray(1, char(*command)) + *parameters;
}

void CanCommandBar::updateEditor()
{
    QString error;
    qsizetype parameterCount = -1;
    const auto payload = commandPayload(error, &parameterCount);
    m_send->setEnabled(m_connected && !payload.isEmpty());
    auto feedback = tr("发送 ID 0x650");
    if (parameterCount >= 0)
    {
        feedback += tr(" · 参数 %1/%2 字节").arg(parameterCount).arg(maxClassicCanPayload - 1);
        if (parameterCount < maxClassicCanPayload)
            feedback += tr(" · DLC %1").arg(parameterCount + 1);
    }
    if (!payload.isEmpty())
        feedback += QStringLiteral(" · ") + QString::fromLatin1(payload.toHex(' ').toUpper());
    if (!error.isEmpty())
        feedback += QStringLiteral(" · ") + error;
    else if (!m_connected)
        feedback += tr(" · 请先连接控制台 CAN");
    else if (payload.isEmpty())
        feedback += tr(" · 请输入命令号");
    m_feedback->setText(feedback);
    const auto style = error.isEmpty() ? QStringLiteral("color: #8a8a8a;")
                                       : QStringLiteral("color: #ffb347;");
    if (m_feedback->styleSheet() != style)
        m_feedback->setStyleSheet(style);
}

void CanCommandBar::submit()
{
    if (!m_connected)
        return;
    QString error;
    const auto payload = commandPayload(error);
    if (!payload.isEmpty())
        emit commandSubmitted(payload);
}

void CanCommandBar::openNumberConverter()
{
    QDialog dialog(this);
    dialog.setObjectName("canNumberConverter");
    dialog.setWindowTitle(tr("十进制 / 十六进制转换"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *decimal = new QLineEdit(&dialog);
    decimal->setObjectName("converterDecimal");
    decimal->setPlaceholderText(tr("例如 1500"));
    form->addRow(tr("十进制数值"), decimal);
    auto *hex = new QLineEdit(&dialog);
    hex->setObjectName("converterHex");
    hex->setPlaceholderText(tr("例如 5DC 或 0x5DC"));
    form->addRow(tr("十六进制数值"), hex);

    auto *options = new QHBoxLayout;
    auto *width = new QComboBox(&dialog);
    width->setObjectName("converterByteWidth");
    width->setAccessibleName(tr("字节长度"));
    width->addItem(tr("自动长度"), 0);
    for (int bytes = 1; bytes < maxClassicCanPayload; ++bytes)
        width->addItem(tr("%1 字节").arg(bytes), bytes);
    options->addWidget(width);
    auto *order = new QComboBox(&dialog);
    order->setObjectName("converterByteOrder");
    order->setAccessibleName(tr("字节序"));
    order->addItem(tr("大端（高字节在前）"), false);
    order->addItem(tr("小端（低字节在前）"), true);
    order->setToolTip(tr("字节序只影响下方的 CAN 参数字节。"));
    options->addWidget(order, 1);
    form->addRow(tr("输出格式"), options);

    auto *outputRow = new QHBoxLayout;
    auto *output = new QLineEdit(&dialog);
    output->setObjectName("converterCanBytes");
    output->setReadOnly(true);
    outputRow->addWidget(output, 1);
    auto *copy = new QPushButton(tr("复制"), &dialog);
    copy->setObjectName("converterCopyBytes");
    copy->setAutoDefault(false);
    outputRow->addWidget(copy);
    auto *outputLabel = new QLabel(tr("CAN 参数字节"), &dialog);
    outputLabel->setBuddy(output);
    form->addRow(outputLabel, outputRow);
    layout->addLayout(form);

    auto *feedback = new QLabel(&dialog);
    feedback->setWordWrap(true);
    layout->addWidget(feedback);
    auto *buttons = new QDialogButtonBox(&dialog);
    buttons->addButton(tr("关闭"), QDialogButtonBox::RejectRole)->setAutoDefault(false);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    std::optional<quint64> value;
    QString inputError;
    const auto refreshOutput = [&]
    {
        QString error = inputError;
        QString result;
        if (value)
        {
            const auto digits = QString::number(*value, 16);
            const auto required = (digits.size() + 1) / 2;
            const auto selected = width->currentData().toInt();
            const auto bytes = selected == 0 ? required : qsizetype(selected);
            if (required >= maxClassicCanPayload)
                error = tr("该数值超过 CAN 参数的 7 字节上限。");
            else if (bytes < required)
                error = tr("所选长度不足，至少需要 %1 字节。").arg(required);
            else
            {
                auto data = QByteArray::fromHex(digits.rightJustified(bytes * 2, QLatin1Char('0')).toLatin1());
                if (order->currentData().toBool())
                    std::reverse(data.begin(), data.end());
                result = QString::fromLatin1(data.toHex(' ').toUpper());
            }
        }
        output->setText(result);
        copy->setEnabled(!result.isEmpty());
        feedback->setText(error);
    };

    const auto convert = [&](const QString &input, int base, QLineEdit *other)
    {
        auto text = input.trimmed();
        if (base == 16)
        {
            if (const auto bytes = parseCanHexBytes(text))
                text = QString::fromLatin1(bytes->toHex());
        }
        bool ok = false;
        const auto parsed = text.toULongLong(&ok, base);
        value.reset();
        inputError.clear();
        if (ok && !text.startsWith('+') && !text.startsWith('-'))
            value = parsed;
        else if (!text.isEmpty())
            inputError = tr("请输入有效的无符号整数。");
        other->setText(value ? QString::number(*value, base == 10 ? 16 : 10).toUpper() : QString());
        refreshOutput();
    };
    connect(decimal, &QLineEdit::textEdited, &dialog, [&](const QString &text) { convert(text, 10, hex); });
    connect(hex, &QLineEdit::textEdited, &dialog, [&](const QString &text) { convert(text, 16, decimal); });
    connect(width, &QComboBox::currentIndexChanged, &dialog, refreshOutput);
    connect(order, &QComboBox::currentIndexChanged, &dialog, refreshOutput);
    connect(copy, &QPushButton::clicked, &dialog, [output]
    {
        QApplication::clipboard()->setText(output->text());
    });
    refreshOutput();
    decimal->setFocus();
    dialog.exec();
}
