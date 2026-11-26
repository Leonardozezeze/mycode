#include "logpanel.h"
#include <QPlainTextEdit>
#include <QBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QFileDialog>
#include <QSaveFile>
#include <QSettings>
#include <QTimer>
#include <QMenu>
#include <QMetaObject>
#include <QThread>

namespace
{
QString levelName(LogPanel::Level level)
{
    switch (level)
    {
    case LogPanel::Level::Debug:
        return QStringLiteral("DEBUG");
    case LogPanel::Level::Info:
        return QStringLiteral("INFO");
    case LogPanel::Level::Warning:
        return QStringLiteral("WARN");
    case LogPanel::Level::Error:
        return QStringLiteral("ERROR");
    }
    return {};
}
QColor levelColor(LogPanel::Level level)
{
    switch (level)
    {
    case LogPanel::Level::Debug:
        return QColor(150, 150, 150);
    case LogPanel::Level::Info:
        return QColor(220, 220, 220);
    case LogPanel::Level::Warning:
        return QColor(255, 179, 71);
    case LogPanel::Level::Error:
        return QColor(239, 83, 80);
    }
    return {};
}
QColor entryColor(LogPanel::Level level, LogPanel::Channel channel)
{
    if (level == LogPanel::Level::Info)
    {
        if (channel == LogPanel::Channel::Receive)
            return QColor(110, 210, 150);
        if (channel == LogPanel::Channel::Transmit)
            return QColor(105, 185, 255);
    }
    return levelColor(level);
}
} // namespace
LogPanel *LogPanel::s_instance = nullptr;

LogPanel::LogPanel(QWidget *parent) : QWidget(parent)
{
    qRegisterMetaType<Level>();
    QSettings settings;
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    auto *toolbar = new QHBoxLayout;
    m_pause = new QPushButton(tr("冻结日志"), this);
    m_pause->setCheckable(true);
    m_pause->setObjectName("freezeLogButton");
    m_pause->setToolTip(tr("只冻结日志视图，控制台收发、波形和记录继续；缓冲仍保留最近 2000 条。"));
    auto *clearButton = new QPushButton(tr("清空"), this);
    auto *save = new QPushButton(tr("保存…"), this);
    auto *saveMenu = new QMenu(save);
    saveMenu->addAction(tr("保存当前筛选"), this, [this] { saveDialog(true); });
    saveMenu->addAction(tr("保存全部缓冲"), this, [this] { saveDialog(false); });
    save->setMenu(saveMenu);
    toolbar->addStretch();
    toolbar->addWidget(m_pause);
    toolbar->addWidget(clearButton);
    toolbar->addWidget(save);
    layout->addLayout(toolbar);
    auto *filters = new QHBoxLayout;
    m_search = new QLineEdit(this);
    m_search->setObjectName("logSearch");
    m_search->setPlaceholderText(tr("搜索日志…"));
    m_search->setClearButtonEnabled(true);
    m_levels = new QComboBox(this);
    m_levels->setObjectName("logLevelFilter");
    m_levels->addItem(tr("全部级别"), -1);
    m_levels->addItem(tr("警告/错误"), int(Level::Warning));
    m_levels->addItem(tr("仅错误"), int(Level::Error));
    m_levels->addItem(tr("仅 INFO"), int(Level::Info));
    m_levels->addItem(tr("仅 DEBUG"), int(Level::Debug));
    m_channels = new QComboBox(this);
    m_channels->setObjectName("logChannelFilter");
    m_channels->addItem(tr("全部来源"), -1);
    m_channels->addItem(tr("控制台 RX"), int(Channel::Receive));
    m_channels->addItem(tr("发送 TX"), int(Channel::Transmit));
    m_channels->addItem(tr("系统"), int(Channel::System));
    filters->addWidget(m_search, 1);
    filters->addWidget(m_levels);
    filters->addWidget(m_channels);
    layout->addLayout(filters);
    auto *display = new QHBoxLayout;
    display->addWidget(new QLabel(tr("接收显示"), this));
    m_receiveMode = new QComboBox(this);
    m_receiveMode->setObjectName("receiveLogMode");
    m_receiveMode->addItems({tr("文本日志"), tr("原始 HEX"), tr("关闭接收日志")});
    m_receiveMode->setCurrentIndex(qBound(0, settings.value("console/receiveMode", 0).toInt(), 2));
    m_receiveMode->setToolTip(tr("文本和 HEX 用于 UART；CAN 按帧显示 ID 和 HEX 数据。“关闭接收日志”同时隐藏两种接收日志。"));
    m_follow = new QCheckBox(tr("跟随"), this);
    m_follow->setObjectName("followLogCheck");
    m_follow->setChecked(true);
    auto *wrap = new QCheckBox(tr("换行"), this);
    wrap->setObjectName("wrapLogCheck");
    wrap->setChecked(settings.value("console/wrap", true).toBool());
    display->addWidget(m_receiveMode);
    display->addStretch();
    display->addWidget(m_follow);
    display->addWidget(wrap);
    layout->addLayout(display);
    m_view = new QPlainTextEdit(this);
    m_view->setObjectName("logView");
    m_view->setReadOnly(true);
    m_view->setMaximumBlockCount(kMaxEntries);
    m_view->setPlaceholderText(tr("暂无匹配日志"));
    m_view->setLineWrapMode(wrap->isChecked() ? QPlainTextEdit::WidgetWidth
                                              : QPlainTextEdit::NoWrap);
    layout->addWidget(m_view, 1);
    m_summary = new QLabel(this);
    m_summary->setObjectName("hintText");
    layout->addWidget(m_summary);
    auto *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(150);
    connect(m_search, &QLineEdit::textChanged, searchTimer, qOverload<>(&QTimer::start));
    connect(searchTimer, &QTimer::timeout, this, &LogPanel::requestRebuild);
    connect(m_levels, &QComboBox::currentIndexChanged, this, &LogPanel::requestRebuild);
    connect(m_channels, &QComboBox::currentIndexChanged, this, &LogPanel::requestRebuild);
    connect(m_pause, &QPushButton::toggled, this,
            [this](bool paused)
            {
                m_frozen.clear();
                m_frozenNew = 0;
                if (paused)
                    for (const auto &entry : m_entries)
                        m_frozen.append(entry);
                m_pause->setText(paused ? tr("继续显示") : tr("冻结日志"));
                requestRebuild();
            });
    connect(m_follow, &QCheckBox::toggled, this,
            [this](bool follow)
            {
                if (follow)
                    m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum());
            });
    connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, this,
            [this](int value)
            {
                if (!m_rendering && value < m_view->verticalScrollBar()->maximum() - 2)
                    m_follow->setChecked(false);
            });
    connect(wrap, &QCheckBox::toggled, this,
            [this](bool on)
            {
                m_view->setLineWrapMode(on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
                QSettings().setValue("console/wrap", on);
            });
    connect(m_receiveMode, &QComboBox::currentIndexChanged, this,
            [this](int index)
            {
                QSettings().setValue("console/receiveMode", index);
                emit receiveModeChanged(receiveMode());
            });
    connect(clearButton, &QPushButton::clicked, this, &LogPanel::clear);
    auto *flush = new QTimer(this);
    flush->setInterval(80);
    connect(flush, &QTimer::timeout, this, &LogPanel::flushDisplay);
    flush->start();
    updateSummary();
}
LogPanel::~LogPanel()
{
    if (s_instance == this)
        s_instance = nullptr;
}
LogPanel *LogPanel::instance()
{
    return s_instance;
}
void LogPanel::setInstance(LogPanel *panel)
{
    s_instance = panel;
}
void LogPanel::debug(const QString &msg)
{
    post(Level::Debug, msg);
}
void LogPanel::info(const QString &msg)
{
    post(Level::Info, msg);
}
void LogPanel::warning(const QString &msg)
{
    post(Level::Warning, msg);
}
void LogPanel::error(const QString &msg)
{
    post(Level::Error, msg);
}
void LogPanel::received(const QString &msg, Level level)
{
    post(level, msg, Channel::Receive);
}
void LogPanel::transmitted(const QString &msg)
{
    post(Level::Info, msg, Channel::Transmit);
}
LogPanel::ReceiveMode LogPanel::receiveMode() const
{
    return ReceiveMode(m_receiveMode->currentIndex());
}
void LogPanel::post(Level level, const QString &msg, Channel channel)
{
    auto *panel = s_instance;
    if (panel)
        QMetaObject::invokeMethod(
            panel, [panel, level, msg, channel] { panel->appendEntry(level, channel, msg); },
            Qt::AutoConnection);
}
void LogPanel::append(Level level, const QString &msg)
{
    appendEntry(level, Channel::System, msg);
}
void LogPanel::appendEntry(Level level, Channel channel, const QString &msg)
{
    if (QThread::currentThread() != thread())
    {
        QMetaObject::invokeMethod(
            this, [this, level, channel, msg] { appendEntry(level, channel, msg); },
            Qt::QueuedConnection);
        return;
    }
    QString text = msg.left(4096);
    text.replace('\r', QStringLiteral("\\r"));
    text.replace('\n', QStringLiteral("\\n"));
    if (msg.size() > 4096)
        text += tr(" …（已截断）");
    m_entries.push_back({m_nextId++, level, channel, QDateTime::currentDateTime(), text});
    if (m_entries.size() > kMaxEntries)
        m_entries.pop_front();
    if (m_pause->isChecked())
        ++m_frozenNew;
    m_dirty = true;
}
QString LogPanel::formatEntry(const Entry &entry) const
{
    const auto channel = entry.channel == Channel::Receive    ? QStringLiteral("[RX]")
                         : entry.channel == Channel::Transmit ? QStringLiteral("[TX]")
                                                              : QString();
    return QStringLiteral("[%1][%2]%3 %4")
        .arg(entry.time.toString("HH:mm:ss.zzz"), levelName(entry.level), channel, entry.message);
}
bool LogPanel::matches(const Entry &entry) const
{
    const int level = m_levels->currentData().toInt(), channel = m_channels->currentData().toInt();
    if (level >= 0 &&
        (level == int(Level::Warning) ? int(entry.level) < level : int(entry.level) != level))
        return false;
    if (channel >= 0 && int(entry.channel) != channel)
        return false;
    return m_search->text().isEmpty() ||
           formatEntry(entry).contains(m_search->text(), Qt::CaseInsensitive);
}
void LogPanel::requestRebuild()
{
    m_rebuild = true;
    m_dirty = true;
}
void LogPanel::flushDisplay()
{
    if (!m_dirty)
        return;
    m_dirty = false;
    m_rendering = true;
    const int oldScroll = m_view->verticalScrollBar()->value();
    if (m_rebuild)
    {
        m_view->clear();
        m_visibleIds.clear();
        m_renderedThrough = 0;
        m_rebuild = false;
    }
    auto appendVisible = [this](const Entry &entry)
    {
        QTextCursor cursor(m_view->document());
        cursor.movePosition(QTextCursor::End);
        if (!m_view->document()->isEmpty())
            cursor.insertBlock();
        QTextCharFormat format;
        format.setForeground(entryColor(entry.level, entry.channel));
        cursor.setCharFormat(format);
        cursor.insertText(formatEntry(entry));
        m_visibleIds.push_back(entry.id);
    };
    if (m_pause->isChecked())
    {
        for (const auto &entry : m_frozen)
            if (entry.id > m_renderedThrough && matches(entry))
                appendVisible(entry);
        if (!m_frozen.isEmpty())
            m_renderedThrough = m_frozen.last().id;
    }
    else
    {
        const auto oldest = m_entries.empty() ? m_nextId : m_entries.front().id;
        while (!m_visibleIds.empty() && m_visibleIds.front() < oldest)
        {
            QTextCursor cursor(m_view->document());
            cursor.movePosition(QTextCursor::Start);
            if (m_visibleIds.size() == 1)
                m_view->clear();
            else
            {
                cursor.select(QTextCursor::BlockUnderCursor);
                cursor.removeSelectedText();
                cursor.deleteChar();
            }
            m_visibleIds.pop_front();
        }
        for (const auto &entry : m_entries)
            if (entry.id > m_renderedThrough && matches(entry))
                appendVisible(entry);
        if (!m_entries.empty())
            m_renderedThrough = m_entries.back().id;
    }
    m_view->verticalScrollBar()->setValue(
        m_follow->isChecked() ? m_view->verticalScrollBar()->maximum() : oldScroll);
    m_rendering = false;
    updateSummary();
}
void LogPanel::updateSummary()
{
    m_summary->setText(m_pause->isChecked()
                           ? tr("显示 %1 条 · 日志冻结期间新增 %2 条（缓存上限 %3）")
                                 .arg(m_visibleIds.size())
                                 .arg(m_frozenNew)
                                 .arg(kMaxEntries)
                           : tr("显示 %1 / %2 条 · 最多保留 %3 条")
                                 .arg(m_visibleIds.size())
                                 .arg(m_entries.size())
                                 .arg(kMaxEntries));
}
void LogPanel::clear()
{
    m_entries.clear();
    m_frozen.clear();
    m_visibleIds.clear();
    m_renderedThrough = 0;
    m_frozenNew = 0;
    m_view->clear();
    m_dirty = false;
    m_rebuild = false;
    updateSummary();
}
bool LogPanel::saveLogTo(const QString &path, bool filtered)
{
    emit aboutToSave();
    QStringList lines;
    if (filtered && m_pause->isChecked())
    {
        for (const auto &entry : m_frozen)
            if (matches(entry))
                lines << formatEntry(entry);
    }
    else
    {
        for (const auto &entry : m_entries)
            if (!filtered || matches(entry))
                lines << formatEntry(entry);
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const auto bytes =
        (lines.join('\n') + (lines.isEmpty() ? QString() : QStringLiteral("\n"))).toUtf8();
    return file.write(bytes) == bytes.size() && file.commit();
}
void LogPanel::saveToFile()
{
    saveDialog(true);
}
void LogPanel::saveDialog(bool filtered)
{
    const auto path = QFileDialog::getSaveFileName(
        this, filtered ? tr("保存当前筛选日志") : tr("保存全部缓冲日志"), QString(),
        tr("文本文件 (*.txt)"));
    if (path.isEmpty())
        return;
    if (saveLogTo(path, filtered))
        info(tr("日志已保存：%1").arg(path));
    else
        error(tr("日志保存失败：%1").arg(path));
}
