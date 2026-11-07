#include "statuspanel.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFontDatabase>
#include <QSpacerItem>
#include <QTime>
#include <QtMath>

namespace {

// 颜色常量：用 QRgb 整数构造，避免 clazy-qcolor-from-literal 警告，
// 也避免每次字符串解析。值可以直接改这里统一配色。
constexpr QRgb kColorAlarm   = 0xef5350;   // 报警/偏差红
constexpr QRgb kColorRunning = 0x66bb6a;   // 运行绿
constexpr QRgb kColorMuted   = 0x9a9a9a;   // 停止/灰色

QString fmt(double value, int decimals, const QString &unit)
{
    if (qIsNaN(value))
        return QStringLiteral("--");
    return QString::number(value, 'f', decimals)
           + QLatin1Char(' ') + unit;
}

} // namespace

StatusPanel::StatusPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("statusPanel");

    // 卡片外观：深色卡片 + 稍亮的描边。想换配色改这里即可。
    setStyleSheet(QStringLiteral(
        "QFrame#statusCard {"
        "  background: #252525;"
        "  border: 1px solid #3a3a3a;"
        "  border-radius: 6px;"
        "}"
        "QLabel#statusCardTitle { color: #9a9a9a; font-size: 12px; }"
        "QFrame#statusCommBar {"
        "  background: #252525;"
        "  border: 1px solid #3a3a3a;"
        "  border-radius: 6px;"
        "}"
        "QLabel#statKey { color: #9a9a9a; }"
        "QLabel#statValue { color: #e0e0e0; font-weight: 600; }"
        ));

    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(8, 8, 8, 8);   // 与 SerialPanel / RecordingPanel 对齐
    grid->setSpacing(8);
    for (int col = 0; col < 4; ++col)
        grid->setColumnStretch(col, 1);

    auto *title = new QLabel(tr("状态"), this);
    title->setObjectName("panelTitle");
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    grid->addWidget(title, 0, 0, 1, 4);

    // 第一排：破碎率 / 转速 / 运行状态
    grid->addWidget(createCard(tr("当前破碎率"), &m_lblCrushRate),    1, 0);
    grid->addWidget(createCard(tr("平均破碎率"), &m_lblCrushAvg),     1, 1);
    grid->addWidget(createCard(tr("电机转速"),   &m_lblRpm),          1, 2);
    grid->addWidget(createCard(tr("运行状态"),   &m_lblRunState),     1, 3);

    // 第二排：含杂率 / 电机
    grid->addWidget(createCard(tr("当前含杂率"), &m_lblImpurityRate), 2, 0);
    grid->addWidget(createCard(tr("平均含杂率"), &m_lblImpurityAvg),  2, 1);
    grid->addWidget(createCard(tr("设定转速"),   &m_lblRpmTarget),     2, 2);
    grid->addWidget(createCard(tr("电机电流"),   &m_lblCurrent),      2, 3);

    // 第三排：通信统计
    auto *commBar = new QFrame(this);
    commBar->setObjectName("statusCommBar");
    auto *commLayout = new QHBoxLayout(commBar);
    commLayout->setContentsMargins(12, 6, 12, 6);
    commLayout->setSpacing(20);
    commLayout->addWidget(createStatItem(tr("帧数"),     &m_lblFrames));
    commLayout->addWidget(createStatItem(tr("错误"),     &m_lblErrors));
    commLayout->addWidget(createStatItem(tr("更新频率"), &m_lblHz));
    commLayout->addStretch();
    commLayout->addWidget(createStatItem(tr("最后更新"), &m_lblLastUpdate));
    grid->addWidget(commBar, 3, 0, 1, 4);

    // 剩余高度留白，避免卡片被纵向拉得很高
    grid->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum,
                                  QSizePolicy::Expanding), 4, 0, 1, 4);

    reset();
}

QFrame *StatusPanel::createCard(const QString &title, QLabel **valueLabel)
{
    auto *card = new QFrame(this);
    card->setObjectName("statusCard");
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("statusCardTitle");

    auto *value = new QLabel(QStringLiteral("--"), card);
    value->setObjectName("statusCardValue");
    QFont valueFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    valueFont.setPointSize(13);   // 卡片窄，别用太大字号否则 "1450 rpm" 会被裁
    valueFont.setBold(true);
    value->setFont(valueFont);

    layout->addWidget(titleLabel);
    layout->addWidget(value);

    *valueLabel = value;
    return card;
}

QFrame *StatusPanel::createStatItem(const QString &key, QLabel **valueLabel)
{
    auto *item = new QFrame(this);
    auto *layout = new QHBoxLayout(item);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *keyLabel = new QLabel(key, item);
    keyLabel->setObjectName("statKey");
    auto *value = new QLabel(QStringLiteral("--"), item);
    value->setObjectName("statValue");

    layout->addWidget(keyLabel);
    layout->addWidget(value);

    *valueLabel = value;
    return item;
}

void StatusPanel::setValue(QLabel *label, const QString &text,
                           const QColor &color)
{
    // QLabel::setText 内部已做相等判断，文本没变是零成本的。
    label->setText(text);

    // 空颜色 = 用样式表里的默认颜色；否则用内联样式覆盖前景色。
    // 只有颜色真正变化时才 setStyleSheet，避免每帧触发样式重解析。
    const QString css = color.isValid()
                            ? QStringLiteral("color: %1;").arg(color.name())
                            : QString();
    if (label->styleSheet() != css)
        label->setStyleSheet(css);
}

void StatusPanel::updateStatus(const MachineStatus &status)
{
    setValue(m_lblCrushRate,    fmt(status.crushRate, 1, "%"));
    setValue(m_lblCrushAvg,     fmt(status.crushRateAvg, 1, "%"));
    setValue(m_lblImpurityRate, fmt(status.impurityRate, 1, "%"));
    setValue(m_lblImpurityAvg,  fmt(status.impurityRateAvg, 1, "%"));

    setValue(m_lblRpm,       fmt(status.rpm, 0, "rpm"));
    setValue(m_lblRpmTarget, fmt(status.rpmTarget, 0, "rpm"));
    setValue(m_lblCurrent,   fmt(status.current, 2, "A"));

    if (status.alarm)
        setValue(m_lblRunState, tr("报警"), QColor(kColorAlarm));
    else if (status.running)
        setValue(m_lblRunState, tr("运行中"), QColor(kColorRunning));
    else
        setValue(m_lblRunState, tr("停止"), QColor(kColorMuted));

    setValue(m_lblFrames, QString::number(status.frames));
    setValue(m_lblErrors, QString::number(status.errors),
             status.errors > 0 ? QColor(kColorAlarm) : QColor());
    setValue(m_lblHz, fmt(status.updateHz, 1, "Hz"));
    setValue(m_lblLastUpdate, QTime::currentTime().toString("HH:mm:ss"));
}

void StatusPanel::reset()
{
    setValue(m_lblCrushRate,    QStringLiteral("--"));
    setValue(m_lblCrushAvg,     QStringLiteral("--"));
    setValue(m_lblImpurityRate, QStringLiteral("--"));
    setValue(m_lblImpurityAvg,  QStringLiteral("--"));
    setValue(m_lblRunState,     tr("停止"), QColor(kColorMuted));
    setValue(m_lblRpm,          QStringLiteral("--"));
    setValue(m_lblRpmTarget,    QStringLiteral("--"));
    setValue(m_lblCurrent,      QStringLiteral("--"));
    setValue(m_lblFrames,     QStringLiteral("0"));
    setValue(m_lblErrors,     QStringLiteral("0"));
    setValue(m_lblHz,         QStringLiteral("--"));
    setValue(m_lblLastUpdate, QStringLiteral("--"));
}