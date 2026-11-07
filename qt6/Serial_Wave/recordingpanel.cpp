#include "recordingpanel.h"
#include "wavepanel.h"
#include "samplearchive.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTimer>

namespace {

constexpr QRgb kColorRunning = 0x66bb6a;   // 有数据在进：绿
constexpr QRgb kColorMuted   = 0x9a9a9a;   // 空闲：灰

QLabel *makeKeyLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName("formLabel");
    return label;
}

QString formatBytes(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    return QString::number(bytes / (1024.0 * 1024.0), 'f', 1)
           + QStringLiteral(" MB");
}

QString formatDuration(double seconds)
{
    const qint64 total = seconds > 0 ? qint64(seconds) : 0;
    return QStringLiteral("%1:%2:%3")
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

RecordingPanel::RecordingPanel(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(tr("采集记录"), this);
    title->setObjectName("panelTitle");
    layout->addWidget(title);

    m_lblState    = new QLabel(QStringLiteral("--"), this);
    m_lblPoints   = new QLabel(QStringLiteral("--"), this);
    m_lblDuration = new QLabel(QStringLiteral("--"), this);
    m_lblRate     = new QLabel(QStringLiteral("--"), this);
    m_lblFileSize = new QLabel(QStringLiteral("--"), this);
    m_lblFlush    = new QLabel(this);
    m_lblFlush->setText(
        tr("%1 KB / 次").arg(SampleArchive::flushThresholdBytes / 1024));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(6);
    form->addRow(makeKeyLabel(tr("状态"),     this), m_lblState);
    form->addRow(makeKeyLabel(tr("采样点"),   this), m_lblPoints);
    form->addRow(makeKeyLabel(tr("时长"),     this), m_lblDuration);
    form->addRow(makeKeyLabel(tr("数据速率"), this), m_lblRate);
    form->addRow(makeKeyLabel(tr("缓冲文件"), this), m_lblFileSize);
    form->addRow(makeKeyLabel(tr("落盘阈值"), this), m_lblFlush);
    layout->addLayout(form);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(6);
    m_autoExportCheck = new QCheckBox(tr("自动导出"), this);
    m_autoExportCheck->setToolTip(
        tr("勾选后，串口断开时自动把全部数据导出到“文档/Serial_Wave”目录"));
    m_exportBtn = new QPushButton(tr("导出…"), this);

    buttonRow->addWidget(m_autoExportCheck);
    buttonRow->addStretch();
    buttonRow->addWidget(m_exportBtn);
    layout->addLayout(buttonRow);

    layout->addStretch();

    connect(m_exportBtn, &QPushButton::clicked,
            this, &RecordingPanel::exportRequested);

    // 状态/速率每隔 0.5 秒刷新一次即可，不需要跟 33 FPS 同频
    m_timer->setInterval(500);
    connect(m_timer, &QTimer::timeout, this, &RecordingPanel::refreshStats);
    m_timer->start();

    setValue(m_lblState, tr("○ 空闲"), QColor(kColorMuted));
}

void RecordingPanel::setWavePanel(WavePanel *wave)
{
    m_wave = wave;
    m_hasBaseline = false;
    m_lastCount = 0;
    refreshStats();
}

bool RecordingPanel::autoExport() const
{
    return m_autoExportCheck && m_autoExportCheck->isChecked();
}

void RecordingPanel::setValue(QLabel *label, const QString &text,
                              const QColor &color)
{
    label->setText(text);

    const QString css = color.isValid()
                            ? QStringLiteral("color: %1;").arg(color.name())
                            : QString();
    if (label->styleSheet() != css)
        label->setStyleSheet(css);
}

void RecordingPanel::refreshStats()
{
    if (!m_wave) {
        setValue(m_lblState, tr("--"));
        setValue(m_lblPoints, QStringLiteral("--"));
        setValue(m_lblDuration, QStringLiteral("--"));
        setValue(m_lblRate, QStringLiteral("--"));
        setValue(m_lblFileSize, QStringLiteral("--"));
        return;
    }

    const qint64 count = m_wave->sampleCount();
    bool receiving = false;

    if (!m_hasBaseline) {
        m_hasBaseline = true;
        m_lastCount = count;
        m_rateClock.start();
    } else {
        const qint64 elapsedMs = m_rateClock.restart();
        const qint64 delta = count - m_lastCount;
        m_lastCount = count;
        receiving = delta > 0;

        const double rate = elapsedMs > 0
                                ? delta * 1000.0 / double(elapsedMs)
                                : 0.0;
        setValue(m_lblRate,
                 QString::number(rate, 'f', 1) + tr(" 点/秒"));
    }

    setValue(m_lblState,
             receiving ? tr("● 记录中") : tr("○ 空闲"),
             receiving ? QColor(kColorRunning) : QColor(kColorMuted));

    setValue(m_lblPoints, QLocale().toString(count));
    setValue(m_lblDuration, formatDuration(m_wave->lastTime()));
    setValue(m_lblFileSize, formatBytes(m_wave->bufferSize()));

    const QString bufferPath = m_wave->bufferFileName();
    if (m_lblFileSize->toolTip() != bufferPath)
        m_lblFileSize->setToolTip(bufferPath);
}
