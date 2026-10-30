#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QCheckBox>
#include "SerialManager.h"
#include "WaveformWidget.h"
#include "FilterEngine.h"
#include "ParameterTrendWindow.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onDataPointReceived(quint64 timestamp, double signal, double grainCount, double lossRate, double fanRpm);
    void onSerialError(const QString &error);
    void onConnectionChanged(bool connected);
    void onRefreshPortsClicked();
    void onFilterTypeChanged(int index);
    void onFilterParamChanged();

private:
    void setupUI();
    void updateParameterDisplay(quint64 timestamp, double signal, double filtered,
                                 double grainCount, double lossRate, double fanRpm);

    // 串口控制
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QPushButton *m_disconnectBtn = nullptr;
    QPushButton *m_refreshBtn = nullptr;

    // 滤波器控制
    QComboBox *m_filterTypeCombo = nullptr;
    QSpinBox *m_windowSizeSpin = nullptr;
    QDoubleSpinBox *m_emaAlphaSpin = nullptr;

    // 显示控制
    QSpinBox *m_timeWindowSpin = nullptr;
    QCheckBox *m_autoScrollCheck = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_trendWindowBtn = nullptr;

    // 参数显示
    QLabel *m_signalValueLabel = nullptr;
    QLabel *m_filteredValueLabel = nullptr;
    QLabel *m_grainCountLabel = nullptr;
    QLabel *m_lossRateLabel = nullptr;
    QLabel *m_fanRpmLabel = nullptr;
    QLabel *m_dataCountLabel = nullptr;

    // 波形显示
    WaveformWidget *m_waveformWidget = nullptr;

    // 串口管理器
    SerialManager *m_serialManager = nullptr;

    // 滤波器
    FilterEngine m_filterEngine;

    // 参数趋势窗口
    ParameterTrendWindow *m_trendWindow = nullptr;

    // 统计数据
    double m_minValue = std::numeric_limits<double>::max();
    double m_maxValue = std::numeric_limits<double>::lowest();
    double m_sumValue = 0.0;
    int m_dataCount = 0;
};
