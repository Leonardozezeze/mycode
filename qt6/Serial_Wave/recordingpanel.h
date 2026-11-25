#ifndef RECORDINGPANEL_H
#define RECORDINGPANEL_H
#include <QWidget>
#include <QColor>
#include <QElapsedTimer>
class QLabel;
class QCheckBox;
class QPushButton;
class WavePanel;
class RecordingPanel : public QWidget
{
    Q_OBJECT
public:
    explicit RecordingPanel(QWidget *parent = nullptr);
    void setWavePanel(WavePanel *wave);
    bool autoExport() const;
signals:
    void exportRequested();
private slots:
    void refreshStats();
    void resetStats();

private:
    void setValue(QLabel *label, const QString &text, const QColor &color = QColor());
    WavePanel *m_wave = nullptr;
    QElapsedTimer m_rateClock;
    qint64 m_lastCount = 0;
    QLabel *m_lblState;
    QLabel *m_lblPoints;
    QLabel *m_lblDuration;
    QLabel *m_lblRate;
    QLabel *m_lblFileSize;
    QLabel *m_lblStarted;
    QCheckBox *m_autoExportCheck;
    QPushButton *m_recordBtn;
};
#endif
