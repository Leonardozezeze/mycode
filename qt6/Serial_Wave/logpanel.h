#ifndef LOGPANEL_H
#define LOGPANEL_H

#include <QWidget>
#include <QMetaType>

class QPlainTextEdit;

class LogPanel : public QWidget
{
    Q_OBJECT
public:
    enum class Level { Debug, Info, Warning, Error };
    Q_ENUM(Level)

    explicit LogPanel(QWidget *parent = nullptr);
    ~LogPanel() override;

    // ---- 全局实例 ----
    static LogPanel *instance();
    static void setInstance(LogPanel *panel);   // 由主窗口调用

    // ---- 静态便捷接口（任意线程可调） ----
    static void debug(const QString &msg);
    static void info(const QString &msg);
    static void warning(const QString &msg);
    static void error(const QString &msg);

public slots:
    void append(LogPanel::Level level, const QString &msg);  // 供信号槽连接
    void clear();
    void saveToFile();

private:
    static void post(Level level, const QString &msg);   // 线程安全投递
    void appendInternal(Level level, const QString &msg); // 只在 GUI 线程跑

    QPlainTextEdit *m_view     = nullptr;
    int             m_maxLines = 2000;

    static LogPanel *s_instance;
};

Q_DECLARE_METATYPE(LogPanel::Level)

#endif // LOGPANEL_H