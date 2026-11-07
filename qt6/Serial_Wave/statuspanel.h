#ifndef STATUSPANEL_H
#define STATUSPANEL_H

#include <QWidget>
#include <QColor>

#include "machinestatus.h"

class QLabel;
class QFrame;

// 状态区：只显示数据，不解析协议。
// 数据层调用 updateStatus() 刷新即可。
class StatusPanel : public QWidget
{
    Q_OBJECT
public:
    explicit StatusPanel(QWidget *parent = nullptr);

public slots:
    void updateStatus(const MachineStatus &status);
    void reset();

private:
    QFrame *createCard(const QString &title, QLabel **valueLabel);
    QFrame *createStatItem(const QString &key, QLabel **valueLabel);
    void setValue(QLabel *label, const QString &text,
                  const QColor &color = QColor());

    // 第一组：破碎率 / 转速 / 运行状态
    QLabel *m_lblCrushRate     = nullptr;
    QLabel *m_lblCrushAvg      = nullptr;
    QLabel *m_lblRpm           = nullptr;
    QLabel *m_lblRunState      = nullptr;

    // 第二组：含杂率 / 电机
    QLabel *m_lblImpurityRate  = nullptr;
    QLabel *m_lblImpurityAvg   = nullptr;
    QLabel *m_lblRpmTarget     = nullptr;
    QLabel *m_lblCurrent       = nullptr;

    // 第三组：通信统计
    QLabel *m_lblFrames     = nullptr;
    QLabel *m_lblErrors     = nullptr;
    QLabel *m_lblHz         = nullptr;
    QLabel *m_lblLastUpdate = nullptr;
};

#endif // STATUSPANEL_H
