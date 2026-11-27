#ifndef SHELLCOMMANDDIALOG_H
#define SHELLCOMMANDDIALOG_H

#include <QByteArray>
#include <QDialog>
#include <QStringList>
#include <optional>

class QComboBox;
class QLabel;
class QPushButton;
class QTableWidget;
class QTimer;

class ShellCommandDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Transport { Uart, Can };
    explicit ShellCommandDialog(QWidget *parent = nullptr);
    void setConnected(Transport transport, bool connected);
    void queryPreferred(Transport preferred);
    void query();
    void receive(const QByteArray &bytes);
    void receiveCan(quint32 id, const QByteArray &payload);
    void sendFailed(const QString &error = {});

signals:
    void queryRequested();
    void canPageRequested(const QByteArray &payload);

private:
    Transport selectedTransport() const;
    bool isConnected(Transport transport) const;
    void updateSelection();
    void requestCanPage();
    void consumeBytes(const QByteArray &bytes);
    void handleLine(const QByteArray &line);
    void finish(const QString &message);

    QLabel *m_status;
    QComboBox *m_transport;
    QPushButton *m_refresh;
    QTableWidget *m_table;
    QTimer *m_timeout;
    QByteArray m_pending;
    QByteArray m_canReport;
    QList<QStringList> m_rows;
    std::optional<quint32> m_canResponder;
    qsizetype m_receivedBytes = 0;
    quint16 m_canPage = 0;
    bool m_inReport = false;
    bool m_uartConnected = false;
    bool m_canConnected = false;
};

#endif
