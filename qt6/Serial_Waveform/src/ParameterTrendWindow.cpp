#include "ParameterTrendWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QStatusBar>
#include <cmath>
#include <cstdlib>

ParameterTrendWindow::ParameterTrendWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
    setWindowTitle(tr("参数趋势 - 破碎率 & 电机转速"));
    resize(900, 500);
}

ParameterTrendWindow::~ParameterTrendWindow() = default;

void ParameterTrendWindow::setupUI()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);

    // 控制面板
    auto *controlGroup = new QGroupBox(tr("控制"));
    auto *controlLayout = new QHBoxLayout(controlGroup);

    auto *timeLayout = new QFormLayout();
    m_timeWindowSpin = new QSpinBox();
    m_timeWindowSpin->setRange(5000, 300000);
    m_timeWindowSpin->setValue(60000);  // 默认 60 秒
    m_timeWindowSpin->setSuffix(tr(" ms"));
    timeLayout->addRow(tr("时间窗口:"), m_timeWindowSpin);
    controlLayout->addLayout(timeLayout);

    m_simulateCheck = new QCheckBox(tr("模拟数据"));
    m_simulateCheck->setChecked(true);
    controlLayout->addWidget(m_simulateCheck);

    m_clearBtn = new QPushButton(tr("清空数据"));
    controlLayout->addWidget(m_clearBtn);

    controlLayout->addStretch();

    mainLayout->addWidget(controlGroup);

    // 趋势图
    m_trendWidget = new ParameterTrendWidget();
    mainLayout->addWidget(m_trendWidget, 1);

    // 状态栏
    statusBar()->showMessage(tr("就绪 - 模拟数据已启动"));

    // 信号连接
    connect(m_timeWindowSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        m_trendWidget->setTimeWindow(value);
    });

    connect(m_clearBtn, &QPushButton::clicked, this, &ParameterTrendWindow::onClearClicked);
    connect(m_simulateCheck, &QCheckBox::toggled, this, &ParameterTrendWindow::onSimulateToggled);

    // 初始化模拟定时器
    m_simulateTimer = new QTimer(this);
    connect(m_simulateTimer, &QTimer::timeout, this, &ParameterTrendWindow::onGenerateData);

    // 启动模拟
    m_targetLossRate = 2.1;
    m_currentLossRate = 2.1;
    m_nextTargetChange = 5000;
    m_simulateTimer->start(50);  // 每 50ms 生成一个数据点
}

void ParameterTrendWindow::addDataPoint(quint64 timestamp, double lossRate, double fanRpm)
{
    // 如果正在模拟，忽略外部数据
    if (m_simulateCheck->isChecked()) return;

    m_trendWidget->addDataPoint(timestamp, lossRate, fanRpm);
}

void ParameterTrendWindow::onClearClicked()
{
    m_trendWidget->clear();
    m_simulateTimestamp = 0;
    statusBar()->showMessage(tr("数据已清空"));
}

void ParameterTrendWindow::onSimulateToggled(bool checked)
{
    if (checked) {
        m_simulateTimer->start(50);
        statusBar()->showMessage(tr("模拟数据已启动"));
    } else {
        m_simulateTimer->stop();
        statusBar()->showMessage(tr("模拟数据已停止"));
    }
}

void ParameterTrendWindow::onGenerateData()
{
    // 生成平滑变化的破碎率 (1.8% - 2.5%)
    double lossRate = generateLossRate(m_simulateTimestamp);

    // 根据破碎率计算电机转速 (0-300 RPM)，非线性反比
    double fanRpm = calculateFanRpm(lossRate, m_simulateTimestamp);

    m_trendWidget->addDataPoint(m_simulateTimestamp, lossRate, fanRpm);

    m_simulateTimestamp += 50;
}

double ParameterTrendWindow::generateLossRate(quint64 timestamp)
{
    // 定期改变目标值（每 8-20 秒）
    if (timestamp >= m_nextTargetChange) {
        // 目标破碎率在 1.8-2.5% 之间
        m_targetLossRate = 1.8 + (std::rand() % 700) / 1000.0;  // 1.8 ~ 2.5

        // 下次改变时间
        quint64 interval = 8000 + (std::rand() % 12000);  // 8-20 秒
        m_nextTargetChange = timestamp + interval;
    }

    // 指数平滑逼近目标（alpha = 0.01，约 5 秒变化 63%）
    double alpha = 0.01;
    m_currentLossRate += alpha * (m_targetLossRate - m_currentLossRate);

    // 添加微小噪声
    double noise = ((std::rand() % 100) - 50) / 1000.0;  // ±0.05%
    double lossRate = m_currentLossRate + noise;

    // 限制范围
    return std::max(1.8, std::min(2.5, lossRate));
}

double ParameterTrendWindow::calculateFanRpm(double lossRate, quint64 timestamp)
{
    // 非线性反比关系：使用二次函数 + 正弦波动
    // lossRate: 1.8 - 2.5
    // fanRpm: 0 - 300

    // 归一化破碎率到 0-1 范围
    double normalizedLoss = (lossRate - 1.8) / (2.5 - 1.8);  // 0 ~ 1

    // 非线性反比：使用 (1 - x^2) 的变形
    // 当 lossRate 高时，rpm 低；当 lossRate 低时，rpm 高
    // 使用 1 - x^1.5 让曲线更自然
    double baseRpm = 300.0 * std::pow(1.0 - normalizedLoss, 1.5);

    // 添加缓慢的正弦波动（让曲线更生动）
    double wave = 20.0 * std::sin(timestamp / 10000.0 * 2 * M_PI);  // 周期 10 秒

    // 添加微小噪声
    double noise = ((std::rand() % 20) - 10) / 10.0;  // ±1 RPM

    double rpm = baseRpm + wave + noise;

    // 限制范围
    return std::max(0.0, std::min(300.0, rpm));
}
