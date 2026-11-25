#ifndef UARTTERMINAL_H
#define UARTTERMINAL_H

#include <QStringConverter>
#include <QTextCursor>
#include <QWidget>

class QComboBox;
class QKeyEvent;
class QLabel;
class UartTerminalScreen;

class UartTerminal : public QWidget
{
    Q_OBJECT
public:
    explicit UartTerminal(QWidget *parent = nullptr);
    void setConnected(bool connected);
    void receive(const QByteArray &bytes);
    void focusTerminal();
    void showWriteError(const QString &message);

signals:
    void inputReady(const QByteArray &bytes);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    enum class EscapeState { Text, Escape, Csi, Osc, OscEscape };

    QByteArray keyBytes(const QKeyEvent &key) const;
    void receiveControl(QChar ch);
    void applyCsi(QChar command);
    void applyColors(const QList<int> &parameters);
    void moveColumn(int column);
    void newLine();
    void writeText(QStringView text);
    void clearScreen();
    void refreshStatus();

    UartTerminalScreen *m_view;
    QComboBox *m_enter;
    QComboBox *m_backspace;
    QLabel *m_status;
    QTextCursor m_output;
    QTextCharFormat m_format;
    QStringDecoder m_decoder{QStringDecoder::Utf8};
    EscapeState m_escape = EscapeState::Text;
    QString m_csi;
    bool m_connected = false;
    bool m_applicationCursorKeys = false;
};

#endif // UARTTERMINAL_H
