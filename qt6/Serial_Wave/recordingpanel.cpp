#include "recordingpanel.h"
#include "wavepanel.h"
#include <QBoxLayout>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTimer>
#include <QSettings>

namespace
{
QString formatBytes(qint64 b)
{
    if (b < 1024)
        return QString::number(b) + QStringLiteral(" B");
    if (b < 1024 * 1024)
        return QString::number(b / 1024.0, 'f', 1) + QStringLiteral(" KB");
    return QString::number(b / (1024. * 1024.), 'f', 1) + QStringLiteral(" MB");
}
QString duration(double seconds)
{
    const qint64 s = qMax(qint64(0), qint64(seconds));
    return QStringLiteral("%1:%2:%3")
        .arg(s / 3600, 2, 10, QLatin1Char('0'))
        .arg((s / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
}
} // namespace
RecordingPanel::RecordingPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(6);
    auto *title = new QLabel(tr("采集会话"), this);
    title->setObjectName("panelTitle");
    layout->addWidget(title);
    auto *form = new QFormLayout;
    form->setVerticalSpacing(4);
    const QStringList keys = {tr("状态"),     tr("采样点"), tr("数据时长"),
                              tr("记录速率"), tr("已保存"), tr("开始时间")};
    const std::array<QLabel **, 6> labels = {&m_lblState, &m_lblPoints,   &m_lblDuration,
                                             &m_lblRate,  &m_lblFileSize, &m_lblStarted};
    for (int i = 0; i < keys.size(); ++i)
    {
        auto *key = new QLabel(keys[i], this);
        key->setObjectName("formLabel");
        *labels[i] = new QLabel(QStringLiteral("--"), this);
        form->addRow(key, *labels[i]);
    }
    layout->addLayout(form);
    auto *buttons = new QHBoxLayout;
    m_recordBtn = new QPushButton(tr("开始记录"), this);
    m_recordBtn->setObjectName("primaryButton");
    auto *newBtn = new QPushButton(tr("新建会话"), this);
    newBtn->setToolTip(tr("保存并结束当前记录，再创建新会话；原文件保留。"));
    auto *exportBtn = new QPushButton(tr("导出…"), this);
    buttons->setSpacing(5);
    buttons->addWidget(m_recordBtn, 1);
    buttons->addWidget(newBtn);
    layout->addLayout(buttons);
    m_autoExportCheck = new QCheckBox(tr("断开时导出 CSV"), this);
    m_autoExportCheck->setToolTip(tr("原始会话始终自动保存；勾选后断开传感连接时额外导出 CSV。"));
    m_autoExportCheck->setChecked(QSettings().value("recording/autoExport", false).toBool());
    auto *directory = new QPushButton(tr("记录目录"), this);
    auto *cleanup = new QPushButton(tr("清理历史…"), this);
    cleanup->setObjectName("clearSavedSessionsButton");
    cleanup->setToolTip(tr("确认后删除记录目录中的历史 .swave 与配套事件文件；保留当前会话和导出文件。"));
    auto *files = new QHBoxLayout;
    files->setSpacing(5);
    for (auto *button : {exportBtn, directory, cleanup})
    {
        button->setProperty("compactAction", true);
        files->addWidget(button);
    }
    layout->addLayout(files);
    layout->addWidget(m_autoExportCheck);
    connect(cleanup, &QPushButton::clicked, this, [this]
    {
        if (m_wave) m_wave->clearSavedSessions();
    });
    layout->addStretch();
    connect(m_recordBtn, &QPushButton::clicked, this,
            [this]
            {
                if (!m_wave)
                    return;
                if (m_wave->isRecording())
                    m_wave->stopRecording();
                else
                    m_wave->startRecording();
            });
    connect(newBtn, &QPushButton::clicked, this,
            [this]
            {
                if (m_wave)
                    m_wave->clearData();
            });
    connect(exportBtn, &QPushButton::clicked, this, &RecordingPanel::exportRequested);
    connect(directory, &QPushButton::clicked, this,
            [this]
            {
                if (m_wave)
                    m_wave->showRecordingDirectory();
            });
    connect(m_autoExportCheck, &QCheckBox::toggled, this,
            [](bool b) { QSettings().setValue("recording/autoExport", b); });
    auto *timer = new QTimer(this);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, this, &RecordingPanel::refreshStats);
    timer->start();
}
void RecordingPanel::setWavePanel(WavePanel *wave)
{
    m_wave = wave;
    connect(wave, &WavePanel::sessionChanged, this, &RecordingPanel::resetStats);
    resetStats();
}
bool RecordingPanel::autoExport() const
{
    return m_autoExportCheck->isChecked();
}
void RecordingPanel::setValue(QLabel *label, const QString &text, const QColor &color)
{
    label->setText(text);
    const QString css =
        color.isValid() ? QStringLiteral("color: %1;").arg(color.name()) : QString();
    if (label->styleSheet() != css)
        label->setStyleSheet(css);
}
void RecordingPanel::resetStats()
{
    m_lastCount = m_wave ? m_wave->sampleCount() : 0;
    m_rateClock.start();
    refreshStats();
}
void RecordingPanel::refreshStats()
{
    if (!m_wave)
        return;
    const qint64 count = m_wave->sampleCount();
    const qint64 elapsed = m_rateClock.isValid() ? m_rateClock.restart() : 0;
    const qint64 delta = qMax(qint64(0), count - m_lastCount);
    m_lastCount = count;
    const double rate = m_wave->isRecording() && elapsed > 0 ? delta * 1000. / elapsed : 0;
    const bool error = m_wave->hasStorageError();
    setValue(m_lblState,
             error                   ? tr("保存失败 · 已停止")
             : m_wave->isPlayback()  ? tr("历史回看")
             : m_wave->isRecording() ? tr("记录中 · 自动保存")
                                     : tr("未记录"),
             QColor(error                   ? 0xef5350
                    : m_wave->isRecording() ? 0x66bb6a
                                            : 0x9a9a9a));
    setValue(m_lblPoints, QLocale().toString(count));
    setValue(m_lblDuration, duration(m_wave->lastTime()));
    setValue(m_lblRate, QString::number(rate, 'f', 1) + tr(" 点/秒"));
    setValue(m_lblFileSize, formatBytes(m_wave->bufferSize()));
    setValue(m_lblStarted, m_wave->startedAt().isValid()
                               ? m_wave->startedAt().toLocalTime().toString("MM-dd HH:mm:ss")
                               : QStringLiteral("--"));
    m_lblFileSize->setToolTip(m_wave->bufferFileName());
    m_recordBtn->setText(m_wave->isRecording() ? tr("停止记录") : tr("开始记录"));
}
