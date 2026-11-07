#ifndef COMMANDBAR_H
#define COMMANDBAR_H

#include <QWidget>

class QLineEdit;
class QPushButton;
class QComboBox;

// 命令输入条：只负责收集用户输入，不直接碰串口。
// 通过 commandSubmitted 把拼好换行符的字节发出去。
class CommandBar : public QWidget
{
    Q_OBJECT
public:
    explicit CommandBar(QWidget *parent = nullptr);

signals:
    void commandSubmitted(const QByteArray &payload);

public slots:
    void setConnected(bool connected);   // 串口断开时禁用输入

private slots:
    void submit();

private:
    QLineEdit   *m_input      = nullptr;
    QPushButton *m_sendBtn    = nullptr;
    QComboBox   *m_terminator = nullptr;   // 行尾换行符
};

#endif // COMMANDBAR_H
