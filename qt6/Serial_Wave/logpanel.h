#ifndef LOGPANEL_H
#define LOGPANEL_H
#include <QWidget>
#include <QMetaType>
#include <QDateTime>
#include <QVector>
#include <deque>

class QPlainTextEdit;
class QLineEdit;
class QComboBox;
class QPushButton;
class QCheckBox;
class QLabel;

class LogPanel : public QWidget
{
    Q_OBJECT
public:
    enum class Level
    {
        Debug,
        Info,
        Warning,
        Error
    };
    Q_ENUM(Level)
    enum class Channel
    {
        System,
        Receive,
        Transmit
    };
    Q_ENUM(Channel)
    enum class ReceiveMode
    {
        Text,
        Hex,
        Disabled
    };
    Q_ENUM(ReceiveMode)

    explicit LogPanel(QWidget *parent = nullptr);
    ~LogPanel() override;
    static LogPanel *instance();
    static void setInstance(LogPanel *panel);
    static void debug(const QString &msg);
    static void info(const QString &msg);
    static void warning(const QString &msg);
    static void error(const QString &msg);
    static void received(const QString &msg, Level level = Level::Info);
    static void transmitted(const QString &msg);
    ReceiveMode receiveMode() const;
    bool saveLogTo(const QString &path, bool filtered = true);

signals:
    void receiveModeChanged(LogPanel::ReceiveMode mode);
    void aboutToSave();

public slots:
    void append(LogPanel::Level level, const QString &msg);
    void appendEntry(LogPanel::Level level, LogPanel::Channel channel, const QString &msg);
    void clear();
    void saveToFile();

private:
    struct Entry
    {
        quint64 id;
        Level level;
        Channel channel;
        QDateTime time;
        QString message;
    };
    static void post(Level level, const QString &msg, Channel channel = Channel::System);
    void flushDisplay();
    void requestRebuild();
    bool matches(const Entry &entry) const;
    QString formatEntry(const Entry &entry) const;
    void updateSummary();
    void saveDialog(bool filtered);
    static constexpr int kMaxEntries = 2000;
    std::deque<Entry> m_entries;
    QVector<Entry> m_frozen;
    std::deque<quint64> m_visibleIds;
    quint64 m_nextId = 1;
    quint64 m_renderedThrough = 0;
    quint64 m_frozenNew = 0;
    bool m_dirty = false;
    bool m_rebuild = false;
    bool m_rendering = false;
    QPlainTextEdit *m_view;
    QLineEdit *m_search;
    QComboBox *m_levels;
    QComboBox *m_channels;
    QComboBox *m_receiveMode;
    QPushButton *m_pause;
    QCheckBox *m_follow;
    QLabel *m_summary;
    static LogPanel *s_instance;
};
Q_DECLARE_METATYPE(LogPanel::Level)
#endif
