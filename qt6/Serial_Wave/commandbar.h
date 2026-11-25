#ifndef COMMANDBAR_H
#define COMMANDBAR_H
#include <QWidget>
#include <QVector>
class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QLabel;
class QMenu;

// 只负责命令编辑和组帧。收藏/历史只填入编辑框，始终由用户点击发送。
class CommandBar : public QWidget
{
    Q_OBJECT
public:
    explicit CommandBar(QWidget *parent = nullptr);
    bool isHexMode() const;
    bool savePreset(const QString &name);
signals:
    void commandSubmitted(const QByteArray &payload);
public slots:
    void setConnected(bool connected);
    void acceptSubmission();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private slots:
    void submit();

private:
    struct Command
    {
        QString name;
        QString text;
        bool hex = false;
        int terminator = 0;
    };
    Command currentCommand() const;
    void applyCommand(const Command &command);
    QByteArray payload(QString *error = nullptr) const;
    void updateEditor();
    void rebuildPresets();
    void writePresets();
    void rebuildHistory();
    QLineEdit *m_input;
    QPushButton *m_sendBtn;
    QComboBox *m_format;
    QComboBox *m_terminator;
    QComboBox *m_presets;
    QCheckBox *m_keepInput;
    QLabel *m_feedback;
    QMenu *m_historyMenu;
    QPushButton *m_historyButton;
    QVector<Command> m_history;
    QVector<Command> m_saved;
    int m_historyIndex = 0;
    Command m_draft;
    bool m_connected = false;
    bool m_applying = false;
};

class CanCommandBar : public QWidget
{
    Q_OBJECT
public:
    explicit CanCommandBar(QWidget *parent = nullptr);
    void setConnected(bool connected);
signals:
    void commandSubmitted(quint32 id, const QByteArray &payload);

private:
    QByteArray commandPayload(QString &error, qsizetype *parameterCount = nullptr) const;
    void updateEditor();
    void submit();
    void openNumberConverter();

    QLineEdit *m_sendId;
    QLineEdit *m_command;
    QLineEdit *m_parameters;
    QPushButton *m_send;
    QLabel *m_feedback;
    bool m_connected = false;
};
#endif
