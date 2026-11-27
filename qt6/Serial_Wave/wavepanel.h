#ifndef WAVEPANEL_H
#define WAVEPANEL_H
#include <QWidget>
#include <QVector>
#include <QFutureWatcher>
#include <QColor>
#include <QElapsedTimer>
#include <array>
#include <deque>
#include <optional>
#include "samplearchive.h"
#include "sessionio.h"
#include "sessionevents.h"

class QCustomPlot;
class QCPItemLine;
class QCPItemText;
class QCPItemRect;
class QTimer;
class QPushButton;
class QLabel;
class QComboBox;
class QProgressDialog;
class QStandardItemModel;
class QDialog;
class QDoubleSpinBox;
class QVBoxLayout;
class QTableWidget;

class WavePanel : public QWidget
{
    Q_OBJECT
public:
    explicit WavePanel(QWidget *parent = nullptr);
    ~WavePanel() override;
    void addData(double x, const QVector<double> &values);
    void addSample(const TelemetrySample &sample);
    qint64 sampleCount() const { return m_archive.count(); }
    double lastTime() const;
    qint64 bufferSize() const { return m_archive.bufferSize(); }
    QString bufferFileName() const { return m_archive.bufferFileName(); }
    QDateTime startedAt() const { return m_archive.startedAt(); }
    bool isRecording() const { return m_recording; }
    bool isPlayback() const { return m_playback; }
    bool hasStorageError() const { return m_storageError; }
    bool flushRecording();
    bool openSession(const QString &path);
    bool exportDataTo(const QString &path); // 后台导出快照，完成结果见 exportFinished
    bool exportRangeTo(const QString &path, double lower, double upper);
    bool selectMeasurementPoint(double time);
    void setConnected(bool connected);
    void cancelExport();
    void setChannelVisible(int channel, bool visible);
    void setChannelColor(int channel, const QColor &color);
    bool setFixedYRange(int plot, double lower, double upper);
    bool addSessionEvent(const QString &type, const QString &message);
    bool saveSessionAs(const QString &path);
    const QVector<SessionEvent> &sessionEvents() const { return m_events.entries(); }
    QElapsedTimer sessionClock() const { return m_eventClock; }

signals:
    void sessionStarted();
    void sessionChanged();
    void storageFailed(const QString &message);
    void exportFinished(bool ok, const QString &path, const QString &error);

public slots:
    void startRecording();
    bool stopRecording();
    void openSessionDialog();
    void chooseRecordingDirectory();
    void showRecordingDirectory();
    void saveSessionAsDialog();
    void exportDataDialog();
    void exportPngDialog();
    void clearData(); // 新建持久会话，旧记录保留
    void resetPlotLayout();
    void configureChannels();
    void addNoteDialog();
    void showEvents();
    void clearSavedSessions();
    void showWholeSession();
    void jumpToTime(double time);
    void clearMeasurement();
    void showMeasurementDetails();
    void exportSelectionDialog();

private slots:
    void onReplotTimeout();
    void togglePause(bool paused);
    void toggleAutoY(bool enabled);
    void loadView();

private:
    void setRange(double lower, double upper);
    void followLatest();
    void setPoints(const QVector<SamplePoint> &points);
    void appendToGraphs(const SamplePoint &point);
    void applyAutoY();
    void requestView();
    void showStats(const ViewData &data);
    void reportStorageError(const QString &message = {});
    void updateControls();
    void updateEventMarkers();
    void rebuildEventModel();
    void appendEventRow(const SessionEvent &event);
    void focusEvent(int row);
    QStringList protectedSessionPaths() const;
    void setupAnalysisTools(QVBoxLayout *layout);
    void createMeasurementItems();
    void updateMeasurementItems();
    void updateAnalysisControls();
    void requestMeasurementRead();
    void loadMeasurement();
    void refreshMeasurementDetails();
    bool startExport(const QString &path, const std::optional<std::array<double, 2>> &range);
    void chooseExportPath(const std::optional<std::array<double, 2>> &range);

    std::array<QCustomPlot *, waveformPlotCount> m_plots{};
    std::array<QCPItemLine *, waveformPlotCount> m_cursors{};
    QTimer *m_replotTimer;
    QTimer *m_saveTimer;
    QTimer *m_viewTimer;
    QPushButton *m_pauseBtn;
    QPushButton *m_autoYBtn;
    QPushButton *m_openSessionBtn;
    QPushButton *m_noteBtn = nullptr;
    QPushButton *m_eventsBtn = nullptr;
    QLabel *m_cursorInfo;
    QLabel *m_sessionInfo;
    QComboBox *m_windowCombo;
    SampleArchive m_archive;
    SessionEvents m_events;
    QElapsedTimer m_eventClock;
    QStandardItemModel *m_eventModel = nullptr;
    QDialog *m_eventsDialog = nullptr;
    std::array<QVector<QCPItemLine *>, waveformPlotCount> m_eventLines;
    QVector<QCPItemText *> m_eventLabels;
    bool m_eventLoadError = false;
    std::deque<SamplePoint> m_recent;
    std::array<double, waveformChannelCount> m_lastChannelTimes;
    QString m_recordingDirectory;
    bool m_recording = false;
    bool m_playback = false;
    bool m_connected = false;
    bool m_paused = false;
    bool m_autoY = true;
    bool m_dirty = false;
    bool m_loadingView = false;
    bool m_storageError = false;
    bool m_showStats = false;
    bool m_viewPending = false;
    bool m_viewBusy = false;
    bool m_exportBusy = false;
    QString m_exportSourcePath;
    QString m_viewSourcePath;
    double m_windowSec = 10;
    std::array<std::array<double, 2>, waveformPlotCount> m_fixedY{{{0, 20}, {0, 3000}, {0, 100}}};
    quint64 m_viewGeneration = 0;
    quint64 m_loadingGeneration = 0;
    std::shared_ptr<std::atomic_bool> m_viewCancel;
    QFutureWatcher<ViewData> m_viewWatcher;
    QFutureWatcher<ExportResult> m_exportWatcher;
    std::shared_ptr<ExportState> m_exportState;
    QProgressDialog *m_exportProgress = nullptr;
    QPushButton *m_measureBtn = nullptr;
    QPushButton *m_measureDetailsBtn = nullptr;
    QPushButton *m_exportSelectionBtn = nullptr;
    QLabel *m_measureInfo = nullptr;
    QDoubleSpinBox *m_jumpTime = nullptr;
    QDialog *m_measureDialog = nullptr;
    QTableWidget *m_measureTable = nullptr;
    QLabel *m_measureDetailsInfo = nullptr;
    std::array<std::array<QCPItemLine *, 2>, waveformPlotCount> m_measureLines{};
    std::array<QCPItemRect *, waveformPlotCount> m_measureBands{};
    std::array<std::array<QCPItemText *, 2>, waveformPlotCount> m_measureLabels{};
    std::optional<double> m_measureARequest, m_measureBRequest;
    MeasurementData m_measurement;
    bool m_measureBusy = false, m_measurePending = false;
    quint64 m_measureGeneration = 0, m_loadingMeasureGeneration = 0;
    QString m_measureSourcePath;
    std::shared_ptr<std::atomic_bool> m_measureCancel;
    QFutureWatcher<MeasurementData> m_measureWatcher;
};
#endif
