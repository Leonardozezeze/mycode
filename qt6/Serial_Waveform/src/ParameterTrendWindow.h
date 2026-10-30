#pragma once

#include <QMainWindow>
#include <QSpinBox>
#include <QPushButton>
#include <QTimer>
#include <QCheckBox>
#include "ParameterTrendWidget.h"

class ParameterTrendWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ParameterTrendWindow(QWidget *parent = nullptr);
    ~ParameterTrendWindow() override;

public slots:
    void addDataPoint(quint64 timestamp, double lossRate, double fanRpm);

private slots:
    void onClearClicked();
    void onGenerateData();
    void onSimulateToggled(bool checked);

private:
    void setupUI();
    double generateLossRate(quint64 timestamp);
    double calculateFanRpm(double lossRate, quint64 timestamp);

    ParameterTrendWidget *m_trendWidget = nullptr;
    QSpinBox *m_timeWindowSpin = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QCheckBox *m_simulateCheck = nullptr;

    QTimer *m_simulateTimer = nullptr;
    quint64 m_simulateTimestamp = 0;

    // 平滑变化的参数
    double m_targetLossRate = 2.1;
    double m_currentLossRate = 2.1;
    quint64 m_nextTargetChange = 0;
};
