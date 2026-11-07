#ifndef RECORDINGPANEL_H
#define RECORDINGPANEL_H

#include <QWidget>
#include <QColor>
#include <QElapsedTimer>

class QLabel;
class QCheckBox;
class QPushButton;
class QTimer;
class WavePanel;

// 采集记录面板：只读展示波形归档的状态和统计，
// 并提供“导出”入口和“断开串口时自动导出”开关。
class RecordingPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RecordingPanel(QWidget *parent = nullptr);

    // 由主窗口在 WavePanel 创建后注入
    void setWavePanel(WavePanel *wave);

    bool autoExport() const;

signals:
    void exportRequested();

private slots:
    void refreshStats();

private:
    void setValue(QLabel *label, const QString &text,
                  const QColor &color = QColor());

    WavePanel *m_wave = nullptr;
    QTimer    *m_timer = nullptr;

    QElapsedTimer m_rateClock;
    qint64 m_lastCount  = 0;
    bool   m_hasBaseline = false;

    QLabel      *m_lblState    = nullptr;
    QLabel      *m_lblPoints   = nullptr;
    QLabel      *m_lblDuration = nullptr;
    QLabel      *m_lblRate     = nullptr;
    QLabel      *m_lblFileSize = nullptr;
    QLabel      *m_lblFlush    = nullptr;
    QCheckBox   *m_autoExportCheck = nullptr;
    QPushButton *m_exportBtn   = nullptr;
};

#endif // RECORDINGPANEL_H
