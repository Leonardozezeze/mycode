#include "uartterminal.h"

#include <QApplication>
#include <QBoxLayout>
#include <QClipboard>
#include <QComboBox>
#include <QFontDatabase>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>

namespace
{
constexpr int terminalLineLimit = 4096;
constexpr QRgb ansiColors[] = {0x181818, 0xe06c75, 0x98c379, 0xe5c07b, 0x61afef, 0xc678dd, 0x56b6c2, 0xdcdfe4};
constexpr QRgb brightColors[] = {0x757d8a, 0xff949d, 0xb8e595, 0xffd891, 0x89c7ff, 0xeaa0ff, 0x89e0e8, 0xffffff};

bool isPrintable(QChar ch)
{
    const auto c = ch.unicode();
    return c >= 0x20 && (c < 0x7f || c >= 0xa0);
}
} // namespace

class UartTerminalScreen : public QPlainTextEdit
{
public:
    explicit UartTerminalScreen(QWidget *parent) : QPlainTextEdit(parent), m_caret(document()) {}

    void updateCaret(const QTextCursor &cursor, bool connected)
    {
        m_caret = cursor;
        m_connected = connected;
        viewport()->update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPlainTextEdit::paintEvent(event);
        if (!m_connected || !hasFocus())
            return;
        auto rect = cursorRect(m_caret);
        rect.setWidth(2);
        if (rect.intersects(viewport()->rect()))
        {
            QPainter painter(viewport());
            painter.fillRect(rect, palette().color(QPalette::Text));
        }
    }

private:
    QTextCursor m_caret;
    bool m_connected = false;
};

UartTerminal::UartTerminal(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(5);
    auto *controls = new QHBoxLayout;
    auto *enterLabel = new QLabel(tr("回车"), this);
    m_enter = new QComboBox(this);
    m_enter->setObjectName("terminalEnter");
    m_enter->addItem(QStringLiteral("CR"), QByteArray("\r"));
    m_enter->addItem(QStringLiteral("LF"), QByteArray("\n"));
    m_enter->addItem(QStringLiteral("CRLF"), QByteArray("\r\n"));
    enterLabel->setBuddy(m_enter);
    controls->addWidget(enterLabel);
    controls->addWidget(m_enter);

    auto *backspaceLabel = new QLabel(tr("退格"), this);
    m_backspace = new QComboBox(this);
    m_backspace->setObjectName("terminalBackspace");
    m_backspace->addItem(QStringLiteral("DEL"), QByteArray(1, '\x7f'));
    m_backspace->addItem(QStringLiteral("BS"), QByteArray(1, '\b'));
    backspaceLabel->setBuddy(m_backspace);
    controls->addWidget(backspaceLabel);
    controls->addWidget(m_backspace);
    controls->addStretch();
    auto *clear = new QPushButton(tr("清屏"), this);
    controls->addWidget(clear);
    layout->addLayout(controls);

    m_view = new UartTerminalScreen(this);
    m_view->setObjectName("uartTerminalView");
    m_view->setAccessibleName(tr("UART 终端"));
    auto terminalFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    terminalFont.setPointSizeF(qMax(qreal(13), terminalFont.pointSizeF() + 2));
    m_view->setFont(terminalFont);
    m_view->setReadOnly(true);
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setMaximumBlockCount(2000);
    m_view->installEventFilter(this);
    m_view->setAttribute(Qt::WA_InputMethodEnabled);
    setFocusProxy(m_view);
    layout->addWidget(m_view, 1);
    m_output = QTextCursor(m_view->document());

    m_status = new QLabel(this);
    m_status->setObjectName("uartTerminalStatus");
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    refreshStatus();

    const QSettings settings;
    m_enter->setCurrentIndex(qBound(0, settings.value("console/terminal/enter", 0).toInt(), 2));
    m_backspace->setCurrentIndex(qBound(0, settings.value("console/terminal/backspace", 0).toInt(), 1));
    connect(m_enter, &QComboBox::currentIndexChanged, this, [](int index)
    { QSettings().setValue("console/terminal/enter", index); });
    connect(m_backspace, &QComboBox::currentIndexChanged, this, [](int index)
    { QSettings().setValue("console/terminal/backspace", index); });
    connect(clear, &QPushButton::clicked, this, &UartTerminal::clearScreen);
}

void UartTerminal::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    if (connected)
    {
        clearScreen();
        m_decoder.resetState();
        m_escape = EscapeState::Text;
        m_csi.clear();
        m_format = {};
        m_applicationCursorKeys = false;
    }
    m_view->updateCaret(m_output, m_connected);
    refreshStatus();
}

void UartTerminal::focusTerminal()
{
    m_view->setFocus();
}

void UartTerminal::showWriteError(const QString &message)
{
    m_status->setText(message);
}

void UartTerminal::refreshStatus()
{
    m_status->setText(m_connected ? tr("已连接 · 点击终端输入 · Ctrl+Shift+V 粘贴")
                                 : tr("请先连接控制台 UART"));
}

QByteArray UartTerminal::keyBytes(const QKeyEvent &key) const
{
    const auto modifiers = key.modifiers() & ~Qt::KeypadModifier;
    if (modifiers == Qt::ControlModifier && key.key() >= Qt::Key_A && key.key() <= Qt::Key_Z)
        return QByteArray(1, char(key.key() - Qt::Key_A + 1));
    if (modifiers != Qt::NoModifier && modifiers != Qt::ShiftModifier)
        return {};

    const QByteArray cursorPrefix = m_applicationCursorKeys ? QByteArrayLiteral("\x1bO") : QByteArrayLiteral("\x1b[");
    switch (key.key())
    {
    case Qt::Key_Return:
    case Qt::Key_Enter: return m_enter->currentData().toByteArray();
    case Qt::Key_Backspace: return m_backspace->currentData().toByteArray();
    case Qt::Key_Tab: return "\t";
    case Qt::Key_Escape: return "\x1b";
    case Qt::Key_Up: return cursorPrefix + 'A';
    case Qt::Key_Down: return cursorPrefix + 'B';
    case Qt::Key_Right: return cursorPrefix + 'C';
    case Qt::Key_Left: return cursorPrefix + 'D';
    case Qt::Key_Home: return cursorPrefix + 'H';
    case Qt::Key_End: return cursorPrefix + 'F';
    case Qt::Key_Delete: return "\x1b[3~";
    default:
        return !key.text().isEmpty() && isPrintable(key.text().front()) ? key.text().toUtf8() : QByteArray();
    }
}

bool UartTerminal::eventFilter(QObject *object, QEvent *event)
{
    if (object != m_view)
        return QWidget::eventFilter(object, event);
    if (event->type() == QEvent::InputMethod && m_connected)
    {
        const auto text = static_cast<QInputMethodEvent *>(event)->commitString();
        if (!text.isEmpty())
        {
            refreshStatus();
            emit inputReady(text.toUtf8());
        }
        return true;
    }
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::ShortcutOverride)
        return QWidget::eventFilter(object, event);

    const auto &key = *static_cast<QKeyEvent *>(event);
    const bool clipboardShortcut = key.modifiers() == (Qt::ControlModifier | Qt::ShiftModifier);
    const bool copy = clipboardShortcut && key.key() == Qt::Key_C;
    const bool paste = m_connected && clipboardShortcut && key.key() == Qt::Key_V;
    QByteArray bytes = m_connected ? keyBytes(key) : QByteArray();
    if (!copy && !paste && bytes.isEmpty())
        return QWidget::eventFilter(object, event);

    event->accept();
    if (event->type() == QEvent::ShortcutOverride)
        return true;
    if (copy)
        m_view->copy();
    else
    {
        if (paste)
        {
            bytes = QApplication::clipboard()->text().toUtf8();
            bytes.replace("\r\n", "\n");
            bytes.replace('\r', '\n');
            bytes.replace("\n", m_enter->currentData().toByteArray());
        }
        if (!bytes.isEmpty())
        {
            refreshStatus();
            emit inputReady(bytes);
        }
    }
    return true;
}

void UartTerminal::receive(const QByteArray &bytes)
{
    if (!m_connected || bytes.isEmpty())
        return;
    const QString text = m_decoder(bytes);
    auto *scroll = m_view->verticalScrollBar();
    const bool follow = scroll->value() == scroll->maximum();
    m_output.beginEditBlock();
    for (qsizetype pos = 0; pos < text.size();)
    {
        if (m_escape == EscapeState::Text && isPrintable(text[pos]))
        {
            const auto start = pos++;
            while (pos < text.size() && isPrintable(text[pos]))
                ++pos;
            writeText(QStringView(text).mid(start, pos - start));
        }
        else
            receiveControl(text[pos++]);
    }
    m_output.endEditBlock();
    if (follow)
        scroll->setValue(scroll->maximum());
    m_view->updateCaret(m_output, m_connected);
}

void UartTerminal::receiveControl(QChar ch)
{
    switch (m_escape)
    {
    case EscapeState::Escape:
        if (ch == '[')
        {
            m_csi.clear();
            m_escape = EscapeState::Csi;
        }
        else
        {
            m_escape = ch == ']' ? EscapeState::Osc : EscapeState::Text;
            if (ch == '7')
            {
                m_savedCursor = m_output;
                m_savedCursor.setKeepPositionOnInsert(true);
            }
            else if (ch == '8' && !m_savedCursor.isNull())
                m_output.setPosition(m_savedCursor.position());
        }
        return;
    case EscapeState::Csi:
        if (ch >= '@' && ch <= '~')
        {
            m_escape = EscapeState::Text;
            applyCsi(ch);
        }
        else if (ch == '\x1b')
            m_escape = EscapeState::Escape;
        else if (m_csi.size() <= 64)
            m_csi.append(ch);
        return;
    case EscapeState::Osc:
        if (ch == '\a')
            m_escape = EscapeState::Text;
        else if (ch == '\x1b')
            m_escape = EscapeState::OscEscape;
        return;
    case EscapeState::OscEscape:
        m_escape = ch == '\\' || ch == '\a' ? EscapeState::Text : EscapeState::Osc;
        return;
    case EscapeState::Text:
        break;
    }
    switch (ch.unicode())
    {
    case '\x1b': m_escape = EscapeState::Escape; break;
    case '\r': moveColumn(0); break;
    case '\n': newLine(); break;
    case '\b': moveColumn(m_output.positionInBlock() - 1); break;
    case '\t': moveColumn((m_output.positionInBlock() / 8 + 1) * 8); break;
    default: break;
    }
}

void UartTerminal::applyCsi(QChar command)
{
    if (m_csi == QStringLiteral("?1") && (command == 'h' || command == 'l'))
    {
        m_applicationCursorKeys = command == 'h';
        return;
    }

    QList<int> parameters;
    for (const auto &entry : m_csi.split(';'))
    {
        bool ok = false;
        const int value = entry.toInt(&ok);
        if (!entry.isEmpty() && (!ok || value < 0))
            return;
        parameters.append(qMin(value, terminalLineLimit));
    }
    const int mode = parameters.value(0);
    const int distance = qMax(1, mode);
    const int column = m_output.positionInBlock();
    switch (command.unicode())
    {
    case 'C': moveColumn(column + distance); break;
    case 'D': moveColumn(column - distance); break;
    case 'G': moveColumn(distance - 1); break;
    case 'K':
        if (mode == 0 || mode == 2)
        {
            if (mode == 2)
                moveColumn(0);
            m_output.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            m_output.removeSelectedText();
            if (mode == 2)
                moveColumn(column);
        }
        else if (mode == 1)
        {
            moveColumn(0);
            const int length = qMin(column + 1, m_output.block().length() - 1);
            writeText(QString(length, ' '));
            moveColumn(column);
        }
        break;
    case 'H':
    case 'f':
    {
        const auto *doc = m_view->document();
        const int height = qMax(1, m_view->viewport()->height() / m_view->fontMetrics().lineSpacing());
        const int first = qMax(0, doc->blockCount() - height);
        const auto block = doc->findBlockByNumber(qMin(doc->blockCount() - 1, first + distance - 1));
        m_output.setPosition(block.position());
        moveColumn(qMax(1, parameters.value(1)) - 1);
        break;
    }
    case 'J':
        if (mode == 2 || mode == 3)
            clearScreen();
        else if (mode == 0)
        {
            m_output.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
            m_output.removeSelectedText();
        }
        break;
    case 'm': applyColors(parameters); break;
    default: break;
    }
}

void UartTerminal::applyColors(const QList<int> &parameters)
{
    for (qsizetype i = 0; i < parameters.size(); ++i)
    {
        const int code = parameters[i];
        if (code == 0)
            m_format = {};
        else if (code == 1 || code == 22)
            m_format.setFontWeight(code == 1 ? QFont::Bold : QFont::Normal);
        else if (code == 39)
            m_format.clearForeground();
        else if (code == 49)
            m_format.clearBackground();
        else if (code >= 30 && code <= 37)
            m_format.setForeground(QColor(ansiColors[code - 30]));
        else if (code >= 90 && code <= 97)
            m_format.setForeground(QColor(brightColors[code - 90]));
        else if (code >= 40 && code <= 47)
            m_format.setBackground(QColor(ansiColors[code - 40]));
        else if (code >= 100 && code <= 107)
            m_format.setBackground(QColor(brightColors[code - 100]));
        else if (code == 38 || code == 48)
        {
            // 跳过扩展颜色参数，避免把颜色分量当成独立的 SGR 指令。
            if (parameters.value(i + 1) == 2)
                i += 4;
            else if (parameters.value(i + 1) == 5)
                i += 2;
        }
    }
}

void UartTerminal::moveColumn(int column)
{
    column = qBound(0, column, terminalLineLimit);
    const auto block = m_output.block();
    const int length = block.length() - 1;
    int offset = qMin(column, length);
    if (offset < length && m_view->document()->characterAt(block.position() + offset).isLowSurrogate())
        --offset;
    m_output.setPosition(block.position() + offset);
    if (column > length)
        m_output.insertText(QString(column - length, ' '), m_format);
}

void UartTerminal::newLine()
{
    m_output.movePosition(QTextCursor::EndOfBlock);
    if (!m_output.movePosition(QTextCursor::NextBlock))
        m_output.insertBlock();
}

void UartTerminal::writeText(QStringView text)
{
    while (!text.isEmpty())
    {
        auto count = qMin(text.size(), qsizetype(terminalLineLimit - m_output.positionInBlock()));
        if (count > 0 && count < text.size() && text[count - 1].isHighSurrogate())
            --count;
        if (count == 0)
        {
            newLine();
            continue;
        }
        const auto block = m_output.block();
        const int end = block.position() + block.length() - 1;
        int replaceEnd = qMin(end, m_output.position() + int(count));
        if (replaceEnd < end && m_view->document()->characterAt(replaceEnd).isLowSurrogate())
            ++replaceEnd;
        m_output.setPosition(replaceEnd, QTextCursor::KeepAnchor);
        m_output.insertText(text.first(count).toString(), m_format);
        text = text.sliced(count);
    }
}

void UartTerminal::clearScreen()
{
    m_savedCursor = {};
    m_output.select(QTextCursor::Document);
    m_output.removeSelectedText();
    m_view->updateCaret(m_output, m_connected);
}
