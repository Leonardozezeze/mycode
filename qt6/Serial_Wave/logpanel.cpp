#include "logpanel.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QFileDialog>
#include <QFile>
#include <QMetaObject>

LogPanel *LogPanel::s_instance = nullptr;

LogPanel::LogPanel(QWidget *parent)
    : QWidget{parent}
{
    qRegisterMetaType<LogPanel::Level>("LogPanel::Level"); // 信号槽跨线程用

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2);
    // 顶部工具条
    auto *toolBar    = new QHBoxLayout();
    auto *titleLabel = new QLabel(tr("日志"), this);
    titleLabel->setObjectName("panelTitle");
    auto *clearBtn   = new QPushButton(tr("清空"), this);
    auto *saveBtn    = new QPushButton(tr("保存"), this);
    toolBar->addWidget(titleLabel);
    toolBar->addStretch();
    toolBar->addWidget(clearBtn);
    toolBar->addWidget(saveBtn);
    mainLayout->addLayout(toolBar);

    // 日志视图
    m_view = new QPlainTextEdit(this);
    m_view->setReadOnly(true);
    m_view->setMaximumBlockCount(m_maxLines);          // 防内存暴涨
    m_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_view->setPlaceholderText(tr("暂无日志"));
    mainLayout->addWidget(m_view, 1);

    connect(clearBtn, &QPushButton::clicked, this, &LogPanel::clear);
    connect(saveBtn,  &QPushButton::clicked, this, &LogPanel::saveToFile);
}

LogPanel::~LogPanel()
{
    if (s_instance == this)
        s_instance = nullptr;
}

LogPanel *LogPanel::instance()               { return s_instance; }
void      LogPanel::setInstance(LogPanel *p) { s_instance = p; }

void LogPanel::debug  (const QString &m) { post(Level::Debug,   m); }
void LogPanel::info   (const QString &m) { post(Level::Info,    m); }
void LogPanel::warning(const QString &m) { post(Level::Warning, m); }
void LogPanel::error  (const QString &m) { post(Level::Error,   m); }

void LogPanel::post(Level level, const QString &msg)
{
    LogPanel *p = s_instance;
    if (!p) return;   // 面板尚未创建，直接丢弃（见后面"缓冲"进阶）

    // 无论调用者处于哪个线程，都投递到面板所属的 GUI 线程执行
    QMetaObject::invokeMethod(p, [p, level, msg]() {
        p->appendInternal(level, msg);
    }, Qt::AutoConnection);
}

void LogPanel::append(Level level, const QString &msg)
{
    appendInternal(level, msg);
}

void LogPanel::appendInternal(Level level, const QString &msg)
{
    QColor  color;
    QString tag;
    // 深色背景下的日志配色
    switch (level) {
    case Level::Debug:   color = QColor(150, 150, 150); tag = "DEBUG"; break;
    case Level::Info:    color = QColor(220, 220, 220); tag = "INFO";  break;
    case Level::Warning: color = QColor(255, 179, 71);  tag = "WARN";  break;
    case Level::Error:   color = QColor(239, 83, 80);   tag = "ERROR"; break;
    }

    const QString ts   = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    const QString line = QStringLiteral("[%1][%2] %3").arg(ts, tag, msg);

    QTextCharFormat fmt;
    fmt.setForeground(color);

    QTextCursor cursor(m_view->document());
    cursor.movePosition(QTextCursor::End);
    if (!m_view->document()->isEmpty())
        cursor.insertBlock();
    cursor.setCharFormat(fmt);
    cursor.insertText(line);

    m_view->verticalScrollBar()->setValue(m_view->verticalScrollBar()->maximum());
}

void LogPanel::clear() { m_view->clear(); }

void LogPanel::saveToFile()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("保存日志"), QString(), tr("文本文件 (*.txt)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error(tr("保存日志失败: %1").arg(f.errorString()));
        return;
    }
    f.write(m_view->toPlainText().toUtf8());
    info(tr("日志已保存: %1").arg(path));
}