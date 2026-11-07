#include "commandbar.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>

CommandBar::CommandBar(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_input   = new QLineEdit(this);
    m_sendBtn = new QPushButton(tr("发送"), this);

    m_terminator = new QComboBox(this);
    m_terminator->addItem(tr("\\r\\n"), QByteArray("\r\n"));
    m_terminator->addItem(tr("\\n"),   QByteArray("\n"));
    m_terminator->addItem(tr("\\r"),   QByteArray("\r"));
    m_terminator->addItem(tr("无"),    QByteArray());

    layout->addWidget(m_input, 1);
    layout->addWidget(m_terminator);
    layout->addWidget(m_sendBtn);

    // 回车和按钮都触发发送
    connect(m_input,   &QLineEdit::returnPressed, this, &CommandBar::submit);
    connect(m_sendBtn, &QPushButton::clicked,     this, &CommandBar::submit);

    setConnected(false);   // 初始未连接
}

void CommandBar::setConnected(bool connected)
{
    m_input->setEnabled(connected);
    m_sendBtn->setEnabled(connected);
    m_terminator->setEnabled(connected);
    m_input->setPlaceholderText(connected
                                ? tr("输入命令，回车发送")
                                : tr("串口未连接"));
}

void CommandBar::submit()
{
    const QString text = m_input->text();
    if (text.isEmpty())
        return;

    QByteArray payload = text.toUtf8();
    payload += m_terminator->currentData().toByteArray();

    emit commandSubmitted(payload);
    m_input->clear();
}
