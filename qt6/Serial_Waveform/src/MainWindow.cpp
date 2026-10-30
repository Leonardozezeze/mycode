#include "MainWindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QMessageBox>
#include <QDoubleSpinBox>
#include <QStatusBar>
#include <limits>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_serialManager(new SerialManager(this))
    , m_trendWindow(new ParameterTrendWindow())
{
    setupUI();

    connect(m_serialManager, &SerialManager::dataPointReceived,
            this, &MainWindow::onDataPointReceived);
    connect(m_serialManager, &SerialManager::errorOccurred,
            this, &MainWindow::onSerialError);
    connect(m_serialManager, &SerialManager::connectionChanged,
            this, &MainWindow::onConnectionChanged);

    onRefreshPortsClicked();
    onConnectionChanged(false);

    setWindowTitle(tr("串口波形显示工具"));
    resize(1200, 800);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);

    // 顶部控制面板
    auto *controlGroup = new QGroupBox(tr("控制面板"));
    auto *controlLayout = new QHBoxLayout(controlGroup);

    // 串口设置
    auto *serialGroup = new QGroupBox(tr("串口设置"));
    auto *serialLayout = new QFormLayout(serialGroup);

    m_portCombo = new QComboBox();
    m_portCombo->setEditable(true);  // 允许手动输入串口路径
    m_portCombo->setInsertPolicy(QComboBox::NoInsert);
    serialLayout->addRow(tr("端口:"), m_portCombo);

    m_baudCombo = new QComboBox();
    m_baudCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    m_baudCombo->setCurrentText("115200");
    serialLayout->addRow(tr("波特率:"), m_baudCombo);

    auto *serialBtnLayout = new QHBoxLayout();
    m_connectBtn = new QPushButton(tr("连接"));
    m_disconnectBtn = new QPushButton(tr("断开"));
    m_refreshBtn = new QPushButton(tr("刷新"));
    serialBtnLayout->addWidget(m_refreshBtn);
    serialBtnLayout->addWidget(m_connectBtn);
    serialBtnLayout->addWidget(m_disconnectBtn);
    serialLayout->addRow(serialBtnLayout);

    controlLayout->addWidget(serialGroup);

    // 滤波器设置
    auto *filterGroup = new QGroupBox(tr("滤波器设置"));
    auto *filterLayout = new QFormLayout(filterGroup);

    m_filterTypeCombo = new QComboBox();
    m_filterTypeCombo->addItems({tr("无"), tr("移动平均"), tr("指数移动平均")});
    filterLayout->addRow(tr("类型:"), m_filterTypeCombo);

    m_windowSizeSpin = new QSpinBox();
    m_windowSizeSpin->setRange(1, 1000);
    m_windowSizeSpin->setValue(50);  // 增大默认窗口
    filterLayout->addRow(tr("窗口大小:"), m_windowSizeSpin);

    m_emaAlphaSpin = new QDoubleSpinBox();
    m_emaAlphaSpin->setRange(0.0, 1.0);
    m_emaAlphaSpin->setSingleStep(0.01);
    m_emaAlphaSpin->setValue(0.05);  // 降低 alpha，更平滑
    m_emaAlphaSpin->setDecimals(2);
    filterLayout->addRow(tr("EMA Alpha:"), m_emaAlphaSpin);

    controlLayout->addWidget(filterGroup);

    // 显示设置
    auto *displayGroup = new QGroupBox(tr("显示设置"));
    auto *displayLayout = new QFormLayout(displayGroup);

    m_timeWindowSpin = new QSpinBox();
    m_timeWindowSpin->setRange(100, 120000);
    m_timeWindowSpin->setValue(5000);
    m_timeWindowSpin->setSuffix(tr(" ms"));
    displayLayout->addRow(tr("时间窗口:"), m_timeWindowSpin);

    m_autoScrollCheck = new QCheckBox(tr("自动滚动"));
    m_autoScrollCheck->setChecked(true);
    displayLayout->addRow(m_autoScrollCheck);

    m_clearBtn = new QPushButton(tr("清空数据"));
    displayLayout->addRow(m_clearBtn);

    m_trendWindowBtn = new QPushButton(tr("打开参数趋势图"));
    m_trendWindowBtn->setStyleSheet("background-color: #2196F3; color: white;");
    displayLayout->addRow(m_trendWindowBtn);

    controlLayout->addWidget(displayGroup);

    // 参数显示
    auto *paramGroup = new QGroupBox(tr("实时参数"));
    auto *paramLayout = new QFormLayout(paramGroup);

    m_signalValueLabel = new QLabel("--");
    m_signalValueLabel->setStyleSheet("font-weight: bold; color: #4CAF50;");
    paramLayout->addRow(tr("信号值:"), m_signalValueLabel);

    m_filteredValueLabel = new QLabel("--");
    m_filteredValueLabel->setStyleSheet("font-weight: bold; color: #FF9800;");
    paramLayout->addRow(tr("滤波值:"), m_filteredValueLabel);

    m_grainCountLabel = new QLabel("0");
    m_grainCountLabel->setStyleSheet("font-weight: bold; color: #2196F3;");
    paramLayout->addRow(tr("籽粒数量:"), m_grainCountLabel);

    m_lossRateLabel = new QLabel("--");
    m_lossRateLabel->setStyleSheet("font-weight: bold; color: #F44336;");
    paramLayout->addRow(tr("损失率:"), m_lossRateLabel);

    m_fanRpmLabel = new QLabel("--");
    m_fanRpmLabel->setStyleSheet("font-weight: bold; color: #9C27B0;");
    paramLayout->addRow(tr("风机转速:"), m_fanRpmLabel);

    m_dataCountLabel = new QLabel("0");
    paramLayout->addRow(tr("数据点数:"), m_dataCountLabel);

    controlLayout->addWidget(paramGroup);

    mainLayout->addWidget(controlGroup);

    // 波形显示区域
    m_waveformWidget = new WaveformWidget();
    mainLayout->addWidget(m_waveformWidget, 1);

    // 状态栏
    statusBar()->showMessage(tr("就绪"));

    // 信号连接
    connect(m_connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshPortsClicked);
    connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onFilterTypeChanged);
    connect(m_windowSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onFilterParamChanged);
    connect(m_emaAlphaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onFilterParamChanged);
    connect(m_timeWindowSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
        m_waveformWidget->setTimeWindow(value);
    });
    connect(m_autoScrollCheck, &QCheckBox::toggled, m_waveformWidget, &WaveformWidget::setAutoScroll);
    connect(m_clearBtn, &QPushButton::clicked, [this]() {
        m_waveformWidget->clear();
        m_filterEngine.reset();
        m_minValue = std::numeric_limits<double>::max();
        m_maxValue = std::numeric_limits<double>::lowest();
        m_sumValue = 0.0;
        m_dataCount = 0;
        m_signalValueLabel->setText("--");
        m_filteredValueLabel->setText("--");
        m_grainCountLabel->setText("0");
        m_lossRateLabel->setText("--");
        m_fanRpmLabel->setText("--");
        m_dataCountLabel->setText("0");
    });

    connect(m_trendWindowBtn, &QPushButton::clicked, [this]() {
        m_trendWindow->show();
        m_trendWindow->raise();
        m_trendWindow->activateWindow();
    });

    onFilterTypeChanged(0);
}

void MainWindow::onConnectClicked()
{
    QString portName = m_portCombo->currentText();
    qint32 baudRate = m_baudCombo->currentText().toInt();

    if (portName.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请选择串口"));
        return;
    }

    if (m_serialManager->open(portName, baudRate)) {
        statusBar()->showMessage(tr("已连接到 %1 @ %2").arg(portName).arg(baudRate));
    }
}

void MainWindow::onDisconnectClicked()
{
    m_serialManager->close();
    statusBar()->showMessage(tr("已断开连接"));
}

void MainWindow::onDataPointReceived(quint64 timestamp, double signal, double grainCount, double lossRate, double fanRpm)
{
    // 应用滤波器
    double filtered = m_filterEngine.process(signal);

    // 添加到波形显示
    m_waveformWidget->addDataPoint(timestamp, signal);
    m_waveformWidget->addFilteredPoint(timestamp, filtered);

    // 添加到参数趋势窗口
    if (lossRate > 0 || fanRpm > 0) {  // 只添加有效数据
        m_trendWindow->addDataPoint(timestamp, lossRate, fanRpm);
    }

    // 更新统计
    m_dataCount++;
    m_sumValue += signal;
    m_minValue = std::min(m_minValue, signal);
    m_maxValue = std::max(m_maxValue, signal);

    // 更新参数显示
    updateParameterDisplay(timestamp, signal, filtered, grainCount, lossRate, fanRpm);
}

void MainWindow::onSerialError(const QString &error)
{
    QMessageBox::critical(this, tr("串口错误"), error);
    statusBar()->showMessage(tr("错误: %1").arg(error));
}

void MainWindow::onConnectionChanged(bool connected)
{
    m_connectBtn->setEnabled(!connected);
    m_disconnectBtn->setEnabled(connected);
    m_portCombo->setEnabled(!connected);
    m_baudCombo->setEnabled(!connected);

    if (connected) {
        statusBar()->showMessage(tr("已连接"));
    } else {
        statusBar()->showMessage(tr("未连接"));
    }
}

void MainWindow::onRefreshPortsClicked()
{
    m_portCombo->clear();
    QStringList ports = SerialManager::availablePorts();
    m_portCombo->addItems(ports);
}

void MainWindow::onFilterTypeChanged(int index)
{
    FilterEngine::FilterType type;
    switch (index) {
    case 1:
        type = FilterEngine::FilterType::MovingAverage;
        m_windowSizeSpin->setEnabled(true);
        m_emaAlphaSpin->setEnabled(false);
        break;
    case 2:
        type = FilterEngine::FilterType::EMA;
        m_windowSizeSpin->setEnabled(false);
        m_emaAlphaSpin->setEnabled(true);
        break;
    default:
        type = FilterEngine::FilterType::None;
        m_windowSizeSpin->setEnabled(false);
        m_emaAlphaSpin->setEnabled(false);
        break;
    }

    m_filterEngine.setFilterType(type);
}

void MainWindow::onFilterParamChanged()
{
    m_filterEngine.setWindowSize(m_windowSizeSpin->value());
    m_filterEngine.setEmaAlpha(m_emaAlphaSpin->value());
}

void MainWindow::updateParameterDisplay(quint64 timestamp, double signal, double filtered,
                                         double grainCount, double lossRate, double fanRpm)
{
    Q_UNUSED(timestamp);

    m_signalValueLabel->setText(QString::number(signal, 'f', 2));
    m_filteredValueLabel->setText(QString::number(filtered, 'f', 2));
    m_grainCountLabel->setText(QString::number(grainCount, 'f', 1));
    m_lossRateLabel->setText(QString::number(lossRate, 'f', 2) + "%");
    m_fanRpmLabel->setText(QString::number(fanRpm, 'f', 0) + " RPM");
    m_dataCountLabel->setText(QString::number(m_dataCount));
}
