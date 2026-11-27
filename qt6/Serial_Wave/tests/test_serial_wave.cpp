#include <QtTest>
#include <QClipboard>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QDockWidget>
#include <QComboBox>
#include <QAction>
#include <QToolBar>
#include <QToolButton>
#include <QSplitter>
#include <QCheckBox>
#include <QMenu>
#include <QTableView>
#include <QTabBar>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QTimer>
#include <QDir>
#include <QScopeGuard>
#include <QtEndian>
#include <algorithm>
#include <cstring>
#include <cmath>
#ifdef Q_OS_LINUX
#include <sys/resource.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#endif
#include "protocolparser.h"
#include "samplearchive.h"
#include "sessionevents.h"
#include "sessionio.h"
#include "sessioncleanup.h"
#include <QMessageBox>
#include <QProgressBar>
#include <QTableWidget>
#include "mainwindow.h"
#include "canpanel.h"
#include "serialpanel.h"
#include "wavepanel.h"
#include "statuspanel.h"
#include "recordingpanel.h"
#include "commandbar.h"
#include "uartterminal.h"
#include "shellcommanddialog.h"
#include "logpanel.h"
#include "qcustomplot.h"
#include "apptheme.h"

namespace
{
// 前两项为串口协议中的原始比例。
QByteArray frame(float crushRatio = 0.1f, float impurityRatio = 0.02f, float rpm = 1450, float target = 1500)
{
    QByteArray b(18, '\0');
    b[0] = char(0xff);
    b[17] = char(0xee);
    const float values[] = {crushRatio, impurityRatio, rpm, target};
    for (int i = 0; i < 4; ++i)
    {
        quint32 bits;
        std::memcpy(&bits, &values[i], 4);
        qToLittleEndian(bits, b.data() + 1 + i * 4);
    }
    return b;
}
QByteArray canQualityFrame(float crushRatio = 0.1f, float impurityRatio = 0.02f)
{
    const auto serial = frame(crushRatio, impurityRatio);
    return serial.mid(5, 4) + serial.mid(1, 4);
}
QByteArray actuatorFrame(float position = 50, ActuatorState state = ActuatorState::Extending)
{
    auto packet = frame().left(17);
    packet[0] = char(0xfe);
    quint32 bits;
    std::memcpy(&bits, &position, 4);
    char bytes[4];
    qToLittleEndian(bits, bytes);
    packet.append(bytes, 4);
    packet.append(char(state));
    packet.append(char(0xee));
    return packet;
}
QString createTestSession(const QString &directory, const QString &name, int channels = 4)
{
    const QString path = QDir(directory).filePath(name + ".swave");
    SampleArchive archive(channels);
    if (!archive.create(path)) return {};
    const double values[] = {1, 2, 1400, 1500, 42.5, double(ActuatorState::Extending)};
    if (!archive.append(0, values) || !archive.flush()) return {};
    SessionEvents events;
    events.begin(path, archive.startedAt());
    if (!events.append(0, "note", name)) return {};
    return path;
}
bool hasText(QWidget *widget, const QString &text)
{
    for (auto *label : widget->findChildren<QLabel *>())
        if (label->text() == text)
            return true;
    return false;
}
} // namespace
class SerialWaveTests : public QObject
{
    Q_OBJECT
    QTemporaryDir m_dir;
private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QCoreApplication::setOrganizationName("SerialWaveTests");
        QCoreApplication::setApplicationName("SerialWaveTests");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_dir.path());
        QSettings().setValue("recording/directory", m_dir.filePath("sessions"));
        applyAppTheme(*qApp);
    }
    void init()
    {
        QSettings settings;
        settings.setValue("command/hex", false);
        settings.setValue("command/terminator", 0);
        settings.setValue("command/keepInput", false);
        settings.remove("command/presets");
        settings.setValue("console/receiveMode", 0);
        settings.remove("console/terminal");
        settings.setValue("serial/autoReconnect", false);
        settings.remove("wave");
    }
    void canPayloadKeepsFrameBoundariesAndDecodesRates()
    {
        ProtocolParser parser;
        QVector<ProtocolFrame> samples;
        parser.setFrameHandler([&](const auto &sample) { samples.append(sample); });
        // 小端 float32：含杂率 0.125，破碎率 0.25。
        const auto packet = QByteArray::fromHex("0000003e0000803e");
        parser.feedCan(packet.left(4));
        parser.feedCan(packet.mid(4));
        parser.feedCan(packet + packet);
        parser.feedCan(QByteArray::fromHex("0000807f0000803e")); // 无穷大
        parser.feedCan(QByteArray::fromHex("0000c07f0000803e")); // NaN
        parser.feedCan(QByteArray::fromHex("0000c03f0000803e")); // 1.5
        QCOMPARE(parser.errorCount(), quint64(6));
        QVERIFY(samples.isEmpty());
        parser.feedCan(packet);
        parser.feedCan(QByteArray::fromHex("000000000000803f")); // 0 与 1
        QCOMPARE(parser.frameCount(), quint64(2));
        QCOMPARE(samples[0].impurityRate, 12.5);
        QCOMPARE(samples[0].crushRate, 25.0);
        QCOMPARE(samples[1].impurityRate, 0.0);
        QCOMPARE(samples[1].crushRate, 100.0);
    }
    void canConsoleBuildsCommandAndParameters()
    {
        CanCommandBar commands;
        auto *id = commands.findChild<QLineEdit *>("canSendId");
        auto *number = commands.findChild<QLineEdit *>("canCommandNumber");
        auto *parameters = commands.findChild<QLineEdit *>("canCommandParameters");
        auto *send = commands.findChild<QPushButton *>("canSendButton");
        auto *feedback = commands.findChild<QLabel *>("canSendFeedback");
        QSignalSpy submitted(&commands, &CanCommandBar::commandSubmitted);
        QCOMPARE(id->text(), QString("0x401"));
        parameters->setText("10 fF");
        QVERIFY(feedback->text().contains("参数 2/7 字节 · DLC 3"));
        number->setText("0x01");
        QVERIFY(feedback->text().contains("DLC 3 · 01 10 FF"));
        QVERIFY(!send->isEnabled());
        QTest::keyClick(parameters, Qt::Key_Return);
        QCOMPARE(submitted.count(), 0);

        commands.setConnected(true);
        for (const auto &invalidNumber : {"", "-1", "256", "0x100"})
        {
            number->setText(invalidNumber);
            QVERIFY(!send->isEnabled());
            QTest::keyClick(parameters, Qt::Key_Return);
        }
        number->setText("0x01");
        for (const auto &invalidParameters : {"ABC", "A B", "A 123", "G1", "0x01"})
        {
            parameters->setText(invalidParameters);
            QVERIFY(!send->isEnabled());
            QTest::keyClick(parameters, Qt::Key_Return);
        }
        parameters->setText("00 01 02 03 04 05 06 07");
        QVERIFY(feedback->text().contains("参数 8/7 字节"));
        QVERIFY(!send->isEnabled());
        QTest::keyClick(parameters, Qt::Key_Return);
        QCOMPARE(submitted.count(), 0);

        parameters->setText("AA 55");
        send->click();
        QCOMPARE(submitted.count(), 1);
        QCOMPARE(submitted[0][0].toUInt(), quint32(0x401));
        QCOMPARE(submitted[0][1].toByteArray(), QByteArray::fromHex("01aa55"));
        QCOMPARE(parameters->text(), QString("AA 55"));

        for (const auto &invalidId : {"", "2048", "0x800", "7FF", "6G1", "+651", "651, 652"})
        {
            id->setText(invalidId);
            QVERIFY(!send->isEnabled());
            QTest::keyClick(parameters, Qt::Key_Return);
        }
        QCOMPARE(submitted.count(), 1);
        id->setText("0x651");
        QVERIFY(feedback->text().contains("发送 ID 0x651"));
        number->setText("255");
        parameters->setText("00112233445566");
        QVERIFY(feedback->text().contains("参数 7/7 字节 · DLC 8"));
        QTest::keyClick(parameters, Qt::Key_Return);
        QCOMPARE(submitted.count(), 2);
        QCOMPARE(submitted[1][0].toUInt(), quint32(0x651));
        QCOMPARE(submitted[1][1].toByteArray(), QByteArray::fromHex("ff00112233445566"));

        id->setText("2047");
        number->setText("0");
        parameters->clear();
        QVERIFY(feedback->text().contains("参数 0/7 字节 · DLC 1"));
        send->click();
        QCOMPARE(submitted.count(), 3);
        QCOMPARE(submitted[2][0].toUInt(), quint32(0x7FF));
        QCOMPARE(submitted[2][1].toByteArray(), QByteArray(1, '\0'));
        id->setText("0");
        send->click();
        QCOMPARE(submitted.count(), 4);
        QCOMPARE(submitted[3][0].toUInt(), quint32(0));
        commands.setConnected(false);
        QVERIFY(!send->isEnabled());
    }
    void canConsoleRemembersLastSendId()
    {
        QSettings settings;
        settings.remove("command/canSendIdText");
        const auto cleanup = qScopeGuard([&] { settings.remove("command/canSendIdText"); });
        {
            CanCommandBar commands;
            commands.setConnected(true);
            auto *id = commands.findChild<QLineEdit *>("canSendId");
            auto *number = commands.findChild<QLineEdit *>("canCommandNumber");
            auto *parameters = commands.findChild<QLineEdit *>("canCommandParameters");
            QCOMPARE(id->text(), QString("0x401"));
            id->setText("1953");
            number->setText("0x02");
            parameters->setText("AA");
            QSignalSpy submitted(&commands, &CanCommandBar::commandSubmitted);
            commands.findChild<QPushButton *>("canSendButton")->click();
            QCOMPARE(submitted.count(), 1);
            QVERIFY(!settings.contains("command/canSendIdText"));
            commands.rememberQueuedSendId(submitted.at(0).at(0).toUInt());
        }
        CanCommandBar restored;
        QCOMPARE(restored.findChild<QLineEdit *>("canSendId")->text(), QString("1953"));

        settings.remove("command/canSendIdText");
        settings.setValue("command/canSendId", "650");
        const auto legacyCleanup = qScopeGuard([&] { settings.remove("command/canSendId"); });
        CanCommandBar migrated;
        QCOMPARE(migrated.findChild<QLineEdit *>("canSendId")->text(), QString("0x650"));

        // 配置里的非法值回退到默认 ID。
        QSettings().setValue("command/canSendIdText", "2048");
        CanCommandBar fallback;
        QCOMPARE(fallback.findChild<QLineEdit *>("canSendId")->text(), QString("0x401"));
    }
    void canNumberConverterFormatsParameterBytes()
    {
        CanCommandBar commands;
        auto *tool = commands.findChild<QPushButton *>("canNumberConverterButton");
        QVERIFY(tool->isEnabled());
        QTimer::singleShot(0, &commands, [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            const auto close = qScopeGuard([&] { dialog->reject(); });
            auto *decimal = dialog->findChild<QLineEdit *>("converterDecimal");
            auto *hex = dialog->findChild<QLineEdit *>("converterHex");
            auto *width = dialog->findChild<QComboBox *>("converterByteWidth");
            auto *order = dialog->findChild<QComboBox *>("converterByteOrder");
            auto *output = dialog->findChild<QLineEdit *>("converterCanBytes");
            auto *copy = dialog->findChild<QPushButton *>("converterCopyBytes");

            QVERIFY(!copy->isEnabled());
            QTest::keyClicks(decimal, "1500");
            QCOMPARE(hex->text(), QString("5DC"));
            QCOMPARE(output->text(), QString("05 DC"));
            order->setCurrentIndex(1);
            QCOMPARE(output->text(), QString("DC 05"));
            copy->click();
            QCOMPARE(QApplication::clipboard()->text(), QString("DC 05"));

            width->setCurrentIndex(width->findData(1));
            QVERIFY(!copy->isEnabled());
            QVERIFY(output->text().isEmpty());
            width->setCurrentIndex(width->findData(4));
            QCOMPARE(output->text(), QString("DC 05 00 00"));

            hex->selectAll();
            QTest::keyClicks(hex, "0xFF");
            QCOMPARE(decimal->text(), QString("255"));
            QCOMPARE(output->text(), QString("FF 00 00 00"));
            width->setCurrentIndex(width->findData(0));
            hex->selectAll();
            QTest::keyClicks(hex, "12 34");
            QCOMPARE(decimal->text(), QString("4660"));
            QCOMPARE(output->text(), QString("34 12"));
            hex->selectAll();
            QTest::keyClicks(hex, "100000000000000");
            QCOMPARE(decimal->text(), QString("72057594037927936"));
            QVERIFY(!copy->isEnabled());
            hex->selectAll();
            QTest::keyClicks(hex, "A B");
            QVERIFY(decimal->text().isEmpty());
            QVERIFY(output->text().isEmpty());
        });
        tool->click();
    }
    void consoleCanReceiveIdsValidateMixedBaseLists()
    {
        QSettings settings;
        const QString legacyKey = QStringLiteral("console/can/id");
        settings.setValue(legacyKey, "100");
        const auto cleanup = qScopeGuard([&] { settings.remove(legacyKey); });
        CanPanel panel(nullptr, CanPanel::Role::Console);
        auto *ids = panel.findChild<QLineEdit *>("canId");
        auto *status = panel.findChild<QLabel *>("canReceiveIdStatus");
        auto *connectButton = panel.findChild<QPushButton *>("primaryButton");
        QCOMPARE(ids->text(), QString("0x064")); // 原来的十进制 100 保持相同 ID。

        ids->setText("000, 0x650 651，0x7fF, 2047");
        QCOMPARE(status->text(), QString("筛选 4 个接收 ID"));
        QVERIFY(connectButton->isEnabled());
        for (const auto &invalid : {"651, G52", "0x", "2048", "0x800", "7FF", "+20", ","})
        {
            ids->setText(invalid);
            QVERIFY(!connectButton->isEnabled());
        }
        ids->clear();
        QCOMPARE(status->text(), QString("接收所有标准数据帧"));
        QVERIFY(connectButton->isEnabled());

        settings.setValue("console/can/receiveIds", "651, 7FF");
        const auto listCleanup = qScopeGuard([&] { settings.remove("console/can/receiveIds"); });
        CanPanel migrated(nullptr, CanPanel::Role::Console);
        QCOMPARE(migrated.findChild<QLineEdit *>("canId")->text(), QString("0x651, 0x7FF"));

#ifdef Q_OS_LINUX
        QStringList many;
        for (int i = 0; i < 512; ++i)
            many << QString::number(i);
        ids->setText(many.join(' '));
        QCOMPARE(status->text(), QString("筛选 512 个接收 ID"));
        QVERIFY(connectButton->isEnabled());
        many << QStringLiteral("512");
        ids->setText(many.join(' '));
        QVERIFY(!connectButton->isEnabled());
#endif
    }
    void canReceptionReachesPlotStatusAndArchive()
    {
        MainWindow window;
        QVERIFY(!window.findChild<QComboBox *>("transportSelector"));
        QVERIFY(!window.findChild<SerialPanel *>("sensorUartPanel"));
        auto *can = window.findChild<CanPanel *>("sensorCanPanel");
        auto *consoleCan = window.findChild<CanPanel *>("consoleCanPanel");
        auto *wave = window.findChild<WavePanel *>();
        auto *status = window.findChild<StatusPanel *>();
        QVERIFY(consoleCan);
        auto *interface = can->findChild<QLineEdit *>("canInterface");
#ifdef Q_OS_LINUX
        QCOMPARE(interface->text(), QString("can0"));
        interface->setText("invalid/name");
        QCOMPARE(can->findChild<QLabel *>("canInterfaceState")->text(), QString("接口名无效"));
        interface->setText("can0");
#else
        QCOMPARE(interface->text(), QString("usb0"));
#endif
        QCOMPARE(can->findChild<QLineEdit *>("canId")->text(), QString("0x100"));
        auto *canCommands = window.findChild<CanCommandBar *>();
        auto *send = canCommands->findChild<QPushButton *>("canSendButton");
        canCommands->findChild<QLineEdit *>("canCommandNumber")->setText("0x01");
        QVERIFY(!wave->isRecording());
        consoleCan->connectionChanged(true);
        QVERIFY(send->isEnabled());
        QVERIFY(!wave->isRecording());
        consoleCan->frameReceived(0x651, QByteArray::fromHex("0000003e0000803e"));
        consoleCan->frameReceived(0x7FF, QByteArray::fromHex("aabb"));
        QCOMPARE(wave->sampleCount(), 0);
        auto *log = window.findChild<LogPanel *>()->findChild<QPlainTextEdit *>();
        QTRY_VERIFY(log->toPlainText().contains("[CAN] 0x651 [8]"));
        QVERIFY(log->toPlainText().contains("[CAN] 0x7FF [2] AA BB"));

        can->connectionChanged(true);
        auto *traffic = can->findChild<QLabel *>("canTraffic");
        auto *lastReceive = can->findChild<QLabel *>("canLastReceive");
        QCOMPARE(traffic->text(), QString("0 帧 · 0 B"));
        QCOMPARE(lastReceive->text(), QString("--"));
        QVERIFY(wave->isRecording());
        can->frameReceived(0x7FE, QByteArray::fromHex("0000003e0000803e"));
        QCOMPARE(wave->sampleCount(), 0);
        can->frameReceived(0x100, QByteArray::fromHex("0000003e0000803e"));
        QCOMPARE(wave->sampleCount(), 1);
        auto *quality = wave->findChild<QCustomPlot *>("qualityPlot");
        QCOMPARE(quality->graph(0)->data()->constBegin()->value, 25.0);
        QCOMPARE(quality->graph(1)->data()->constBegin()->value, 12.5);
        QVERIFY(hasText(status, QString("25.0 %")));
        QVERIFY(hasText(status, QString("12.5 %")));
        QVERIFY(hasText(status, QString("暂无电机数据")));
        QCOMPARE(status->findChild<QLabel *>("actuatorStateValue")->text(), QString("暂无数据"));
        QTRY_COMPARE(traffic->text(), QString("2 帧 · 16 B")); // 注入信号绕过设备接收过滤。
        QVERIFY(QTime::fromString(lastReceive->text(), "HH:mm:ss.zzz").isValid());
        can->frameReceived(0x101, QByteArray::fromHex("00000c4300001643"));
        can->frameReceived(0x100, QByteArray::fromHex("0000803e0000003e"));
        can->frameReceived(0x102, QByteArray::fromHex("00002a4202"));
        QCOMPARE(wave->sampleCount(), 4);
        QVERIFY(hasText(status, QString("140 rpm")));
        QVERIFY(hasText(status, QString("150 rpm")));
        QVERIFY(hasText(status, QString("18.8 %")));
        QCOMPARE(status->findChild<QLabel *>("actuatorPositionValue")->text(), QString("42.5 %"));
        QCOMPARE(quality->graph(0)->dataCount(), 2);
        auto *speedPlot = wave->findChild<QCustomPlot *>("speedPlot");
        QCOMPARE(speedPlot->graph(0)->dataCount(), 1);
        QCOMPARE(speedPlot->graph(0)->data()->constBegin()->value, 140.0);

        // 注入的是连接信号，底层套接字还没打开。失败时日志说明原因，不能记为已排队。
        const auto previousSendId = QSettings().value("command/canSendIdText");
        canCommands->findChild<QLineEdit *>("canSendId")->setText("0x7A1");
        auto *canData = canCommands->findChild<QLineEdit *>("canCommandParameters");
        canData->setText("00 01");
        canCommands->findChild<QPushButton *>("canSendButton")->click();
        QCOMPARE(canData->text(), QString("00 01"));
        QTRY_VERIFY(log->toPlainText().contains("CAN 0x7A1 发送失败"));
        QCOMPARE(QSettings().value("command/canSendIdText"), previousSendId);
        const auto beforeDisconnect = wave->sessionEvents();
        QVERIFY(std::none_of(beforeDisconnect.begin(), beforeDisconnect.end(), [](const auto &event)
        { return event.type == "command"; }));

        can->connectionChanged(false);
        QVERIFY(!wave->isRecording());
        const auto events = wave->sessionEvents();
        QVERIFY(std::any_of(events.begin(), events.end(), [](const auto &event)
        { return event.type == "disconnect" && event.message.contains("CAN"); }));
        SampleArchive archive;
        QVERIFY(archive.open(wave->bufferFileName()));
        QCOMPARE(archive.count(), 4);
        QVector<double> values;
        double time = 0;
        QVERIFY(archive.nearestSample(0, time, values));
        QCOMPARE(values.size(), sampleChannelCount);
        QCOMPARE(values[0], 25.0);
        QCOMPARE(values[1], 12.5);
        QVERIFY(std::all_of(values.begin() + 2, values.end(),
                            [](double value) { return qIsNaN(value); }));
        QVector<double> recordedSpeeds;
        QVERIFY(archive.forEachSample([&](double, const double *row)
        {
            if (qIsFinite(row[2]))
                recordedSpeeds.append(row[2]);
            return true;
        }));
        QCOMPARE(recordedSpeeds, QVector<double>({140.0}));
        can->connectionChanged(true);
        QCOMPARE(traffic->text(), QString("0 帧 · 0 B"));
        QCOMPARE(lastReceive->text(), QString("--"));
        can->connectionChanged(false);
        QVERIFY(send->isEnabled());
        can->connectionChanged(true);
        consoleCan->connectionChanged(false);
        QVERIFY(!send->isEnabled());
        QVERIFY(wave->isRecording());
        can->connectionChanged(false);
    }
    void canIdDialogValidatesAndPersistsMappings()
    {
        QSettings settings;
        const QStringList keys{"can/id", "can/speedId", "can/actuatorId",
                               "console/can/receiveIdText", "command/canSendIdText"};
        const auto cleanup = qScopeGuard([&]
        {
            for (const auto &key : keys)
                settings.remove(key);
        });
        MainWindow window;
        auto *action = window.findChild<QAction *>("configureCanIdsAction");
        QVERIFY(action);
        QTimer::singleShot(0, &window, [&]
        {
            auto *dialog = window.findChild<QDialog *>("canIdConfigurationDialog");
            QVERIFY(dialog);
            const auto close = qScopeGuard([&]
            {
                if (dialog->isVisible())
                    dialog->reject();
            });
            auto *quality = dialog->findChild<QLineEdit *>("canQualityId");
            auto *speed = dialog->findChild<QLineEdit *>("canSpeedId");
            auto *actuator = dialog->findChild<QLineEdit *>("canActuatorId");
            auto *tx = dialog->findChild<QLineEdit *>("canShellTxId");
            auto *rx = dialog->findChild<QLineEdit *>("canShellRxIds");
            auto *box = dialog->findChild<QDialogButtonBox *>();
            QCOMPARE(quality->text(), QString("0x100"));
            QCOMPARE(speed->text(), QString("0x101"));
            QCOMPARE(actuator->text(), QString("0x102"));
            QCOMPARE(tx->text(), QString("0x401"));
            QCOMPARE(rx->text(), QString("0x402"));
            quality->setText("257");
            box->button(QDialogButtonBox::Save)->click();
            QVERIFY(dialog->isVisible());
            quality->setText("300");
            speed->setText("0x12D");
            actuator->setText("0x12E");
            tx->setText("0x650");
            rx->setText("0x651, 1620");
            box->button(QDialogButtonBox::Save)->click();
            QVERIFY(!dialog->isVisible());
        });
        action->trigger();
        auto *sensor = window.findChild<CanPanel *>("sensorCanPanel");
        QCOMPARE(sensor->sensorFrameIndex(300).value_or(-1), 0);
        QCOMPARE(sensor->sensorFrameIndex(301).value_or(-1), 1);
        CanPanel restored;
        QCOMPARE(restored.sensorFrameIndex(302).value_or(-1), 2);
        QCOMPARE(settings.value("console/can/receiveIdText").toString(), QString("0x651, 1620"));
        QCOMPARE(settings.value("command/canSendIdText").toString(), QString("0x650"));
    }
    void parserRatioValuesBecomePercentages()
    {
        ProtocolParser p;
        ProtocolFrame latest;
        MachineStatus status;
        p.setFrameHandler([&](const auto &f) { latest = f; });
        p.setStatusHandler([&](const auto &s) { status = s; });

        // 与 Rust encode_frame(0.02, 0.03, 140, 150) 相同的固定字节样例。
        const auto packet = QByteArray::fromHex("ff0ad7a33c8fc2f53c00000c4300001643ee");
        p.feed(packet.left(7));
        QCOMPARE(p.frameCount(), quint64(0));
        p.feed(packet.mid(7));
        QCOMPARE(p.frameCount(), quint64(1));
        QCOMPARE(p.errorCount(), quint64(0));
        QVERIFY(qAbs(latest.crushRate - 2.0) < 1e-5);
        QVERIFY(qAbs(latest.impurityRate - 3.0) < 1e-5);
        QCOMPARE(latest.rpm, 140.0);
        QCOMPARE(latest.targetRpm, 150.0);
        QVERIFY(qAbs(status.crushRate - 2.0) < 1e-5);
        QVERIFY(qAbs(status.impurityRate - 3.0) < 1e-5);

        p.feed(frame(0.04f, 0.05f, 140, 150));
        QVERIFY(qAbs(status.crushRateAvg - 3.0) < 1e-5);
        QVERIFY(qAbs(status.impurityRateAvg - 4.0) < 1e-5);

        p.resetSession();
        p.feed(frame(0.021f, 0.03f, 140, 150));
        QVERIFY(qAbs(latest.crushRate - 2.1) < 1e-5);
        QVERIFY(qAbs(latest.impurityRate - 3.0) < 1e-5);

        p.feed(frame(0.0f, 1.0f, 0, 0));
        QCOMPARE(latest.crushRate, 0.0);
        QCOMPARE(latest.impurityRate, 100.0);
    }
    void parserFragmentationAndSessionTime()
    {
        for (int cut = 1; cut < 18; ++cut)
        {
            ProtocolParser p;
            p.feed(frame().left(cut));
            QCOMPARE(p.frameCount(), 0);
            p.feed(frame().mid(cut));
            QCOMPARE(p.frameCount(), 1);
        }
        ProtocolParser p;
        QString text;
        p.setTextHandler([&](const auto &bytes) { text += QString::fromUtf8(bytes); });
        ProtocolFrame latest;
        p.setFrameHandler([&](auto &f) { latest = f; });
        p.feed(QByteArray("ready\n") + frame() + frame());
        QCOMPARE(p.frameCount(), 2);
        QCOMPARE(text, QString("ready\n"));
        QCOMPARE(latest.targetRpm, 1500.0);
        const double before = latest.time;
        QTest::qWait(100);
        p.resetStream();
        p.feed(frame());
        QVERIFY(latest.time - before >= .08);
        p.resetSession();
        p.feed(frame(0.5f));
        QCOMPARE(latest.index, quint64(0));
        QVERIFY(latest.time < .05);
        MachineStatus status;
        p.setStatusHandler([&](auto &s) { status = s; });
        p.feed(frame(0.3f));
        QVERIFY(qAbs(status.crushRateAvg - 40.0) < 1e-5);
    }
    void archivePersistenceAndRecovery()
    {
        const auto path = m_dir.filePath("persistent.swave");
        {
            SampleArchive archive;
            QVERIFY(archive.create(path));
            double values[] = {1, 2, 3, 4};
            QVERIFY(archive.append(0, values));
            QVERIFY(archive.append(2.5, values));
            QVERIFY(archive.flush());
            QVERIFY(!archive.append(1, values));
            QCOMPARE(archive.count(), 2);
        }
        QVERIFY(QFile::exists(path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::Append));
        QCOMPARE(file.write("partial"), 7);
        file.close();
        SampleArchive opened;
        QVERIFY(opened.open(path));
        QCOMPARE(opened.count(), 2);
        QCOMPARE(opened.ignoredTailBytes(), 7);
        double x;
        QVERIFY(opened.lastX(x));
        QCOMPARE(x, 2.5);
        int count = 0;
        double foundTime = 0, foundTarget = 0;
        QVERIFY(opened.forEachSampleInRange(2, 3,
                                            [&](double t, const double *v)
                                            {
                                                foundTime = t;
                                                foundTarget = v[3];
                                                ++count;
                                                return true;
                                            }));
        QCOMPARE(count, 1);
        QCOMPARE(foundTime, 2.5);
        QCOMPARE(foundTarget, 4.0);
        QVERIFY(!opened.open(m_dir.filePath("missing.swave")));
        QCOMPARE(opened.count(), 2);
    }
    void exportSnapshotCancellationAndTarget()
    {
        const auto source = m_dir.filePath("export_source.swave");
        SampleArchive archive;
        QVERIFY(archive.create(source));
        double values[] = {10, 2, 1450, 1500};
        for (int i = 0; i < 200; ++i)
            QVERIFY(archive.append(i * .1, values));
        QVERIFY(archive.flush());
        auto state = std::make_shared<ExportState>();
        state->total = 100;
        auto path = m_dir.filePath("data.json");
        auto result = exportSession(source, path, state);
        QVERIFY2(result.ok, qPrintable(result.error));
        QFile json(path);
        QVERIFY(json.open(QIODevice::ReadOnly));
        const auto doc = QJsonDocument::fromJson(json.readAll());
        QCOMPARE(doc.object()["samples"].toArray().size(), 100);
        QCOMPARE(doc.object()["samples"].toArray()[0].toObject()["target_speed"].toDouble(),
                 1500.0);
        path = m_dir.filePath("data.csv");
        result = exportSession(source, path, state);
        QVERIFY(result.ok);
        QFile csv(path);
        QVERIFY(csv.open(QIODevice::ReadOnly));
        const auto original = csv.readAll();
        QVERIFY(original.contains("target_speed"));
        csv.close();
        state->cancelled.store(true);
        result = exportSession(source, path, state);
        QVERIFY(result.cancelled);
        QVERIFY(csv.open(QIODevice::ReadOnly));
        QCOMPARE(csv.readAll(), original);
        state->cancelled.store(false);
        result = exportSession(source, source, state);
        QVERIFY(!result.ok);
    }
    void saveAsIncludesPendingAndKeepsOriginal()
    {
        const auto original = m_dir.filePath("original.swave"), copy = m_dir.filePath("copy.swave");
        SampleArchive archive;
        QVERIFY(archive.create(original));
        double values[] = {1, 2, 3, 4};
        QVERIFY(archive.append(0, values));
        QVERIFY(archive.flush());
        QVERIFY(archive.append(1, values));
        QVERIFY(archive.saveAs(copy));
        QCOMPARE(archive.count(), 2);
        QCOMPARE(archive.bufferFileName(), copy);
        QVERIFY(archive.append(2, values));
        QVERIFY(archive.flush());
        SampleArchive old;
        QVERIFY(old.open(original));
        QCOMPARE(old.count(), 1);
        SampleArchive saved;
        QVERIFY(saved.open(copy));
        QCOMPARE(saved.count(), 3);
    }
    void failedWriteCanBeSavedElsewhere()
    {
#ifdef Q_OS_LINUX
        const auto original = m_dir.filePath("limited.swave"),
                   recovered = m_dir.filePath("recovered.swave");
        SampleArchive archive;
        QVERIFY(archive.create(original));
        double values[] = {1, 2, 3, 4};
        QVERIFY(archive.append(0, values));
        QVERIFY(archive.append(1, values));
        struct rlimit previous;
        QCOMPARE(getrlimit(RLIMIT_FSIZE, &previous), 0);
        const auto handler = signal(SIGXFSZ, SIG_IGN);
        auto restore = qScopeGuard(
            [&]
            {
                setrlimit(RLIMIT_FSIZE, &previous);
                signal(SIGXFSZ, handler);
            });
        auto limited = previous;
        limited.rlim_cur = 64;
        QCOMPARE(setrlimit(RLIMIT_FSIZE, &limited), 0);
        QVERIFY(!archive.flush());
        QCOMPARE(archive.count(), 2);
        QVERIFY(!archive.errorString().isEmpty());
        QCOMPARE(setrlimit(RLIMIT_FSIZE, &previous), 0);
        QVERIFY2(archive.saveAs(recovered), qPrintable(archive.errorString()));
        SampleArchive saved;
        QVERIFY(saved.open(recovered));
        QCOMPARE(saved.count(), 2);
        double last;
        QVERIFY(saved.lastX(last));
        QCOMPARE(last, 1.0);
#else
        QSKIP("此写入失败模拟使用 Linux 文件大小限制。");
#endif
    }
    void boundedViewRetainsPeaksAndStats()
    {
        const auto path = m_dir.filePath("long.swave");
        SampleArchive archive;
        QVERIFY(archive.create(path));
        for (int i = 0; i < 30000; ++i)
        {
            double values[] = {i == 12345 ? 100. : 1., 2., 1450., 1500.};
            QVERIFY(archive.append(i * .01, values));
        }
        QVERIFY(archive.flush());
        auto cancel = std::make_shared<std::atomic_bool>(false);
        const auto view = readSessionView(path, 0, 300, cancel);
        QVERIFY(view.error.isEmpty());
        QCOMPARE(view.count, 30000);
        QVERIFY(view.points.size() <= 4000);
        QCOMPARE(view.stats[0].max, 100.0);
        bool peak = false;
        for (const auto &p : view.points)
            if (p.values[0] == 100)
                peak = true;
        QVERIFY(peak);
        WavePanel panel;
        QVERIFY(panel.openSession(path));
        auto *quality = panel.findChild<QCustomPlot *>("qualityPlot");
        quality->xAxis->setRange(0, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(quality->graph(0)->dataCount() > 0 &&
                                     (quality->graph(0)->data()->constEnd() - 1)->key > 299.9,
                                 3000);
        // 抽稀后的点距可以很大，但连续的原始数据不能被画成断线。
        for (const auto &point : *quality->graph(0)->data())
            QVERIFY(qIsFinite(point.value));
    }
    void disconnectedAndStaleStatus()
    {
        StatusPanel panel;
        panel.setConnected(true);
        MachineStatus status;
        status.running = true;
        status.rpm = 1450;
        status.rpmTarget = 1500;
        panel.updateStatus(status);
        QVERIFY(hasText(&panel, QString::fromUtf8("运行中")));
        QVERIFY(hasText(&panel, QString("1450 rpm")));
        QVERIFY(hasText(&panel, QString("1500 rpm")));
        QVERIFY(hasText(&panel, QString("-50 rpm")));
        QTest::qWait(2100);
        panel.refreshFreshness();
        QVERIFY(hasText(&panel, QString::fromUtf8("数据过期")));
        QVERIFY(!hasText(&panel, QString("1450 rpm")));
        QVERIFY(!hasText(&panel, QString("1500 rpm")));
        QVERIFY(!hasText(&panel, QString("-50 rpm")));
        panel.updateStatus(status);
        QVERIFY(hasText(&panel, QString("1450 rpm")));
        panel.setConnected(false);
        QVERIFY(hasText(&panel, QString::fromUtf8("未连接")));
        QVERIFY(!hasText(&panel, QString::fromUtf8("运行中")));
        QVERIFY(!hasText(&panel, QString("1450 rpm")));
        panel.setConnected(true);
        QVERIFY(hasText(&panel, QString::fromUtf8("等待数据")));
    }
    void consoleCanReopenAndOldLayoutMigrates()
    {
        QSettings settings;
        // 模拟上一版：控制台在底部且已关闭的布局。
        QMainWindow legacy;
        auto *oldDock = new QDockWidget(&legacy);
        oldDock->setObjectName("consoleDock");
        legacy.addDockWidget(Qt::BottomDockWidgetArea, oldDock);
        oldDock->hide();
        settings.setValue("window/layoutVersion", 0);
        settings.setValue("window/state", legacy.saveState());
        MainWindow w;
        w.show();
        w.resize(1600, 1000);
        auto *dock = w.findChild<QDockWidget *>("consoleDock");
        auto *action = w.findChild<QAction *>("showConsoleAction");
        auto *toolbar = w.findChild<QToolBar *>("viewToolbar");
        QVERIFY(dock && action && toolbar);
        QCOMPARE(w.dockWidgetArea(dock), Qt::RightDockWidgetArea);
        QTRY_VERIFY(dock->isVisible());
        QTRY_VERIFY(dock->width() >= 460);
        QVERIFY(dock->close());
        QVERIFY(!dock->isVisible());
        QVERIFY(!action->isChecked());
        QVERIFY(action->isEnabled());
        auto *button = qobject_cast<QToolButton *>(toolbar->widgetForAction(action));
        QVERIFY(button && button->isVisible());
        QVERIFY(!button->isChecked());
        QTest::mouseClick(button, Qt::LeftButton);
        QTRY_VERIFY(dock->isVisible());
        QVERIFY(action->isChecked());
        dock->close();
        w.activateWindow();
        w.setFocus();
        QTRY_VERIFY(w.isActiveWindow());
        QTest::keyClick(&w, Qt::Key_L, Qt::ControlModifier);
        QTRY_VERIFY(dock->isVisible());
        w.addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->close();
        w.resetLayout();
        QCOMPARE(w.dockWidgetArea(dock), Qt::RightDockWidgetArea);
        QTRY_VERIFY(dock->isVisible());
        QTRY_VERIFY(w.findChild<QCustomPlot *>("qualityPlot")->height() >= 150);
        QTRY_VERIFY(w.findChild<QCustomPlot *>("speedPlot")->height() >= 150);
        w.close();
        // 当前版仍记住用户主动关闭的状态，且关闭后始终保留入口。
        MainWindow restored;
        restored.show();
        auto *restoredDock = restored.findChild<QDockWidget *>("consoleDock");
        restoredDock->close();
        restored.close();
        MainWindow hidden;
        hidden.show();
        QVERIFY(!hidden.findChild<QDockWidget *>("consoleDock")->isVisible());
        hidden.findChild<QAction *>("showConsoleAction")->trigger();
        QTRY_VERIFY(hidden.findChild<QDockWidget *>("consoleDock")->isVisible());
        hidden.close();
    }
    void timeAxisNeverShowsNegativeTime()
    {
        WavePanel panel;
        panel.resize(1000, 650);
        panel.show();
        auto *quality = panel.findChild<QCustomPlot *>("qualityPlot");
        auto *speed = panel.findChild<QCustomPlot *>("speedPlot");
        quality->xAxis->setRange(-5, 5);
        QCOMPARE(quality->xAxis->range().lower, 0.0);
        QCOMPARE(quality->xAxis->range().upper, 10.0);
        QCOMPARE(speed->xAxis->range().lower, 0.0);
        speed->xAxis->setRange(-30, -20);
        QCOMPARE(speed->xAxis->range().lower, 0.0);
        QCOMPARE(speed->xAxis->range().upper, 10.0);
        QCOMPARE(quality->xAxis->range().upper, 10.0);
        QTest::qWait(50);
        const QPoint center = quality->axisRect()->rect().center();
        QWheelEvent wheel(QPointF(center), QPointF(quality->mapToGlobal(center)), QPoint(),
                          QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(quality, &wheel);
        QVERIFY(quality->xAxis->range().lower >= 0);
        QVERIFY(quality->xAxis->range().size() > 10);
        QCOMPARE(quality->xAxis->range().lower, speed->xAxis->range().lower);
        QCOMPARE(quality->xAxis->range().upper, speed->xAxis->range().upper);
    }
    void recordingFreezeReplayAndExit()
    {
        MainWindow w;
        w.show();
        auto *sensor = w.findChild<CanPanel *>("sensorCanPanel");
        auto *wave = w.findChild<WavePanel *>();
        auto *rec = w.findChild<RecordingPanel *>();
        sensor->connectionChanged(true);
        QVERIFY(wave->isRecording());
        for (int i = 0; i < 20; ++i)
        {
            sensor->frameReceived(0x100, canQualityFrame());
            QTest::qWait(2);
        }
        const auto path = wave->bufferFileName();
        QCOMPARE(wave->sampleCount(), 20);
        QVERIFY(wave->flushRecording());
        QVERIFY(QMetaObject::invokeMethod(wave, "togglePause", Q_ARG(bool, true)));
        sensor->frameReceived(0x100, canQualityFrame());
        QCOMPARE(wave->sampleCount(), 21);
        QVERIFY(QMetaObject::invokeMethod(wave, "togglePause", Q_ARG(bool, false)));
        auto *quality = wave->findChild<QCustomPlot *>("qualityPlot");
        const auto plottedBeforeStop = quality->graph(0)->dataCount();
        QCOMPARE(plottedBeforeStop, 21);
        auto *recordButton = rec->findChild<QPushButton *>("primaryButton");
        QVERIFY(recordButton);
        recordButton->click();
        QVERIFY(!wave->isRecording());
        sensor->frameReceived(0x100, canQualityFrame());
        QCOMPARE(wave->sampleCount(), 21);
        QCOMPARE(quality->graph(0)->dataCount(), plottedBeforeStop);
        wave->clearData();
        QCOMPARE(wave->sampleCount(), 0);
        sensor->frameReceived(0x100, canQualityFrame(0.4f));
        QCOMPARE(wave->sampleCount(), 1);
        QVERIFY(wave->lastTime() < .05);
        QMetaObject::invokeMethod(rec, "refreshStats");
        for (auto *l : rec->findChildren<QLabel *>())
            if (l->text().contains(QString::fromUtf8("点/秒")))
                QVERIFY(!l->text().startsWith('-'));
        sensor->connectionChanged(false);
        QVERIFY(!wave->isRecording());
        QVERIFY(wave->openSession(path));
        QVERIFY(wave->isPlayback());
        QCOMPARE(wave->sampleCount(), 21);
        QTRY_VERIFY_WITH_TIMEOUT(
            wave->findChildren<QCustomPlot *>().first()->graph(0)->dataCount() > 0, 3000);
        const auto exported = m_dir.filePath("gui.csv");
        QSignalSpy spy(wave, &WavePanel::exportFinished);
        QVERIFY(wave->exportDataTo(exported));
        QTRY_COMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toBool());
        const auto plots = wave->findChildren<QCustomPlot *>();
        QCOMPARE(plots.size(), 3);
        QCOMPARE(plots[1]->graphCount(), 2);
        w.close();
        QVERIFY(QFile::exists(path));
        SampleArchive persisted;
        QVERIFY(persisted.open(path));
        QCOMPARE(persisted.count(), 21);
    }
    void canShellCommandListUsesPagedUtf8Replies()
    {
        ShellCommandDialog dialog;
        dialog.setConnected(ShellCommandDialog::Transport::Can, true);
        QSignalSpy requests(&dialog, &ShellCommandDialog::canPageRequested);
        QSignalSpy uartRequests(&dialog, &ShellCommandDialog::queryRequested);
        dialog.queryPreferred(ShellCommandDialog::Transport::Uart);
        QCOMPARE(uartRequests.count(), 0);
        QCOMPARE(requests.count(), 1);
        QCOMPARE(requests[0][0].toByteArray(), QByteArray::fromHex("050000"));

        QByteArray report("shell-info begin\r\n"
                          "command name=ping opcode=0x01 help=检查设备是否在线\r\n"
                          "command name=speed opcode=none help=设置目标转速\r\n"
                          "shell-info end\r\n");
        report.append(QByteArray((4 - report.size() % 4) % 4, '\n'));
        const auto reply = [](quint16 page, const QByteArray &bytes)
        {
            auto response = QByteArray::fromHex("05000000");
            qToLittleEndian(page, response.data() + 2);
            return response + bytes;
        };
        auto *table = dialog.findChild<QTableWidget *>("shellCommandTable");
        const int pages = int(report.size() / 4);
        for (int page = 0; page < pages; ++page)
        {
            QCOMPARE(requests.count(), page + 1);
            const auto request = requests[page][0].toByteArray();
            QCOMPARE(request[0], '\x05');
            QCOMPARE(qFromLittleEndian<quint16>(request.constData() + 1), quint16(page));
            const auto response = reply(quint16(page), report.mid(4 * page, 4));
            if (page == 1)
            {
                dialog.receiveCan(0x403, response);
                dialog.receiveCan(0x402, reply(0, report.left(4)));
                QCOMPARE(requests.count(), 2);
            }
            dialog.receiveCan(0x402, response);
        }
        QCOMPARE(table->rowCount(), 0);
        QCOMPARE(requests.count(), pages + 1);
        dialog.receiveCan(0x402, reply(quint16(pages), {}));
        QCOMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, 2)->text(), QString("检查设备是否在线"));
        QCOMPARE(table->item(1, 1)->text(), QString("仅 UART"));

        dialog.query();
        dialog.receiveCan(0x402, QByteArray::fromHex("0501"));
        QVERIFY(dialog.findChild<QLabel *>("shellCommandStatus")->text().contains("0x01"));
        QCOMPARE(table->rowCount(), 0);
    }
    void closeFlushesPendingAndCommandFailureKeepsText()
    {
        QString path;
        {
            MainWindow w;
            auto *wave = w.findChild<WavePanel *>();
            wave->startRecording();
            path = wave->bufferFileName();
            wave->addData(.2, {1, 2, 3, 4});
            QCOMPARE(wave->sampleCount(), 1);
            w.close();
        }
        SampleArchive archive;
        QVERIFY(archive.open(path));
        QCOMPARE(archive.count(), 1);
        CommandBar command;
        command.setConnected(true);
        auto *input = command.findChild<QLineEdit *>();
        input->setText("test");
        QMetaObject::invokeMethod(&command, "submit");
        QCOMPARE(input->text(), QString("test"));
        command.acceptSubmission();
        QVERIFY(input->text().isEmpty());
        QTest::keyClick(input, Qt::Key_Up);
        QCOMPARE(input->text(), QString("test"));
    }
    void pseudoTerminalRoundTrip()
    {
#ifdef Q_OS_LINUX
        const int consoleMaster = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        QVERIFY(consoleMaster >= 0);
        auto cleanupConsole = qScopeGuard([&] { ::close(consoleMaster); });
        QCOMPARE(grantpt(consoleMaster), 0);
        QCOMPARE(unlockpt(consoleMaster), 0);
        const auto consoleName = QString::fromLocal8Bit(ptsname(consoleMaster));
        MainWindow w;
        auto *sensor = w.findChild<CanPanel *>("sensorCanPanel");
        sensor->connectionChanged(true);
        auto *wave = w.findChild<WavePanel *>();
        QVERIFY(wave->isRecording());
        auto *console = w.findChild<SerialPanel *>("consoleUartPanel");
        auto *consolePorts = console->findChild<QComboBox *>("serialPortCombo");
        consolePorts->addItem(consoleName, consoleName);
        consolePorts->setCurrentIndex(consolePorts->count() - 1);
        QVERIFY(QMetaObject::invokeMethod(console, "onOpenClicked"));
        QVERIFY(console->isOpen());
        const QByteArray statusText("device ready\n");
        QCOMPARE(::write(consoleMaster, statusText.constData(), statusText.size()), statusText.size());
        sensor->frameReceived(0x100, canQualityFrame());
        sensor->frameReceived(0x101, frame().mid(9, 8));
        sensor->frameReceived(0x100, canQualityFrame());
        QCOMPARE(wave->sampleCount(), 3);
        QTRY_VERIFY(hasText(w.findChild<StatusPanel *>(), QString::fromUtf8("运行中")));
        QTRY_VERIFY(
            w.findChild<LogPanel *>()->findChild<QPlainTextEdit *>()->toPlainText().contains(
                "device ready"));
        auto *command = w.findChild<CommandBar *>();
        auto *input = command->findChild<QLineEdit *>();
        input->setText("ping");
        QVERIFY(QMetaObject::invokeMethod(command, "submit"));
        QVERIFY(input->text().isEmpty());
        QByteArray received;
        auto gotReply = [&]
        {
            char bytes[128];
            const auto size = ::read(consoleMaster, bytes, sizeof(bytes));
            if (size > 0)
                received.append(bytes, size);
            return received.contains("ping\r\n");
        };
        QTRY_VERIFY_WITH_TIMEOUT(gotReply(), 2000);
        QTRY_COMPARE(console->transmittedBytes(), quint64(6));
        command->findChild<QComboBox *>("sendFormatCombo")->setCurrentIndex(1);
        input->setText("AA 55 00 FF");
        QVERIFY(QMetaObject::invokeMethod(command, "submit"));
        auto gotHex = [&]
        {
            char bytes[128];
            const auto count = ::read(consoleMaster, bytes, sizeof(bytes));
            if (count > 0)
                received.append(bytes, count);
            return received.endsWith(QByteArray::fromHex("aa5500ff"));
        };
        QTRY_VERIFY_WITH_TIMEOUT(gotHex(), 2000);
        QCOMPARE(received, QByteArray("ping\r\n") + QByteArray::fromHex("aa5500ff"));
        QTRY_COMPARE(console->transmittedBytes(), quint64(10));
        QVector<QString> commandEvents;
        for (const auto &event : wave->sessionEvents())
            if (event.type == "command")
                commandEvents.append(event.message);
        QCOMPARE(commandEvents.size(), 2);
        QVERIFY(commandEvents[0].contains("ping\r\n"));
        QVERIFY(commandEvents[1].contains("[HEX] AA 55 00 FF"));

        auto *terminal = w.findChild<UartTerminal *>();
        auto *terminalView = terminal->findChild<QPlainTextEdit *>("uartTerminalView");
        auto *tabs = w.findChild<QTabBar *>("consoleTabs");
        auto *commandPage = w.findChild<QWidget *>("consoleCommandPage");
        auto *logPanel = w.findChild<LogPanel *>();
        w.show();
        w.findChild<QDockWidget *>("consoleDock")->show();
        QCOMPARE(tabs->count(), 4);
        QCOMPARE(tabs->tabText(1), QString("UART 终端"));
        QCOMPARE(tabs->tabText(2), QString("CAN"));
        tabs->setCurrentIndex(0);
        QVERIFY(command->isVisible());
        tabs->setCurrentIndex(1);
        QVERIFY(commandPage->isHidden());
        QTRY_VERIFY(terminalView->hasFocus());
        logPanel->findChild<QComboBox *>("receiveLogMode")->setCurrentIndex(2);
        const QByteArray shellOutput("\x1b[32mshell\x1b[0m> ");
        QCOMPARE(::write(consoleMaster, shellOutput.constData(), shellOutput.size()), shellOutput.size());
        QTRY_VERIFY(terminalView->toPlainText().endsWith("shell> "));
        QVERIFY(!logPanel->findChild<QPlainTextEdit *>("logView")->toPlainText().contains("shell> "));

        QTest::keyClicks(terminalView, "help");
        QTest::keyClick(terminalView, Qt::Key_Return);
        QTest::keyClick(terminalView, Qt::Key_Up);
        QTest::keyClick(terminalView, Qt::Key_Tab);
        QTest::keyClick(terminalView, Qt::Key_Backspace);
        QTest::keyClick(terminalView, Qt::Key_C, Qt::ControlModifier);
        QTest::keyClick(terminalView, Qt::Key_F, Qt::ControlModifier);
        terminal->findChild<QComboBox *>("terminalEnter")->setCurrentText("CRLF");
        terminal->findChild<QComboBox *>("terminalBackspace")->setCurrentText("BS");
        QTest::keyClick(terminalView, Qt::Key_Return);
        QTest::keyClick(terminalView, Qt::Key_Backspace);
        QApplication::clipboard()->setText("one\r\ntwo\n");
        QTest::keyClick(terminalView, Qt::Key_V, Qt::ControlModifier | Qt::ShiftModifier);
        const QByteArray expectedKeys("help\r\x1b[A\t\x7f\x03\x06\r\n\bone\r\ntwo\r\n");
        auto gotKeys = [&]
        {
            char bytes[128];
            const auto count = ::read(consoleMaster, bytes, sizeof(bytes));
            if (count > 0)
                received.append(bytes, count);
            return received.endsWith(expectedKeys);
        };
        QTRY_VERIFY_WITH_TIMEOUT(gotKeys(), 2000);
        QCOMPARE(tabs->currentIndex(), 1);
        QCOMPARE(wave->sampleCount(), 3);
        w.findChild<QAction *>("searchLogAction")->trigger();
        QCOMPARE(tabs->currentIndex(), 0);
        QVERIFY(commandPage->isVisible());
        QVERIFY(logPanel->findChild<QLineEdit *>("logSearch")->hasFocus());
        tabs->setCurrentIndex(2);
        QVERIFY(w.findChild<CanCommandBar *>()->isVisible());
        QTRY_VERIFY(logPanel->y() < commandPage->height() / 2);
        const auto compactScreenshot = qEnvironmentVariable("SERIAL_WAVE_COMPACT_CONSOLE_SCREENSHOT");
        if (!compactScreenshot.isEmpty())
            QVERIFY(commandPage->grab().save(compactScreenshot));

        auto *getCommands = w.findChild<QPushButton *>("getShellCommands");
        getCommands->click();
        auto *shellDialog = w.findChild<QDialog *>("shellCommandDialog");
        QVERIFY(shellDialog->isVisible());
        const QByteArray query("shell-info\r");
        QTRY_VERIFY_WITH_TIMEOUT(([&]
        {
            char bytes[64];
            const auto count = ::read(consoleMaster, bytes, sizeof(bytes));
            if (count > 0)
                received.append(bytes, count);
            return received.endsWith(query);
        }()), 2000);
        auto *table = shellDialog->findChild<QTableWidget *>("shellCommandTable");
        logPanel->findChild<QComboBox *>("receiveLogMode")->setCurrentIndex(2);
        QSignalSpy replyChunks(console, &SerialPanel::dataReceived);
        const QByteArray firstReply("> shell-info\r\nshell-info begin\r\n"
                                    "can_request_id=0x401 can_reply_id=0x402\r\n"
                                    "command name=ping opcode=0x01 help=检查设备是否在线\r\n"
                                    "command name=speed opcode=none help=");
        const auto help = QString("设置目标转速").toUtf8();
        const auto fragment = firstReply + help.left(1);
        QCOMPARE(::write(consoleMaster, fragment.constData(), fragment.size()), fragment.size());
        QTRY_VERIFY(!replyChunks.isEmpty());
        QCOMPARE(table->rowCount(), 0);
        const auto remainder = help.mid(1) + "\r\nshell-info end\r\n> ";
        QCOMPARE(::write(consoleMaster, remainder.constData(), remainder.size()), remainder.size());
        QTRY_COMPARE(table->rowCount(), 2);
        QCOMPARE(table->item(0, 0)->text(), QString("ping"));
        QCOMPARE(table->item(0, 1)->text(), QString("0x01"));
        QCOMPARE(table->item(1, 0)->text(), QString("speed"));
        QCOMPARE(table->item(1, 1)->text(), QString("仅 UART"));
        QCOMPARE(table->item(1, 2)->text(), QString("设置目标转速"));
        const auto tableScreenshot = qEnvironmentVariable("SERIAL_WAVE_COMMAND_TABLE_SCREENSHOT");
        if (!tableScreenshot.isEmpty())
            QVERIFY(shellDialog->grab().save(tableScreenshot));
        shellDialog->close();

        QCOMPARE(tabs->tabText(3), QString("快速设置"));
        tabs->setCurrentIndex(3);
        auto *quick = w.findChild<QWidget *>("quickSettingsPage");
        QVERIFY(quick->isVisible());
        QVERIFY(!logPanel->isVisible());
        int pendingCount = 0;
        for (auto *button : quick->findChildren<QPushButton *>())
        {
            if (!button->property("pendingParameter").toBool())
                continue;
            QVERIFY(!button->isEnabled());
            ++pendingCount;
        }
        QCOMPARE(pendingCount, 13);
        const auto screenshot = qEnvironmentVariable("SERIAL_WAVE_QUICK_SETTINGS_SCREENSHOT");
        if (!screenshot.isEmpty())
            QVERIFY(quick->grab().save(screenshot));
        auto *logToggle = quick->findChild<QPushButton *>("quickLogToggle");
        logToggle->click();
        QVERIFY(logPanel->isVisible());
        const auto logScreenshot = qEnvironmentVariable("SERIAL_WAVE_LOG_SCREENSHOT");
        if (!logScreenshot.isEmpty())
            QVERIFY(commandPage->grab().save(logScreenshot));
        logToggle->click();
        QVERIFY(!logPanel->isVisible());
        tabs->setCurrentIndex(2);
        QVERIFY(logPanel->isVisible());
        tabs->setCurrentIndex(3);
        QVERIFY(!logPanel->isVisible());
        w.findChild<QAction *>("searchLogAction")->trigger();
        QVERIFY(logPanel->isVisible());
        logToggle->click();
        auto *target = quick->findChild<QDoubleSpinBox *>("quickTargetRpm");
        auto *setSpeed = quick->findChild<QPushButton *>("quickSetSpeed");
        auto *readSpeed = quick->findChild<QPushButton *>("quickReadSpeed");
        auto *pwm = quick->findChild<QSpinBox *>("quickManualPwm");
        auto *setPwm = quick->findChild<QPushButton *>("quickSetPwm");
        auto *pid = quick->findChild<QComboBox *>("quickPidMode");
        auto *setPid = quick->findChild<QPushButton *>("quickSetPid");
        auto *readPid = quick->findChild<QPushButton *>("quickReadPid");
        QCOMPARE(target->minimum(), -250.0);
        QCOMPARE(target->maximum(), 250.0);
        target->setValue(-123.25);
        QVERIFY(setSpeed->isEnabled());
        QVERIFY(readSpeed->isEnabled());
        setSpeed->click();
        readSpeed->click();
        QCOMPARE(pwm->minimum(), -1000);
        QCOMPARE(pwm->maximum(), 1000);
        pwm->setValue(-321);
        setPwm->click();
        pid->setCurrentIndex(1);
        setPid->click();
        pid->setCurrentIndex(0);
        setPid->click();
        readPid->click();
        const QByteArray quickCommands("speed -123.25\r" "speed\r" "pwm -321\r" "pid on\r" "pid off\r" "pid\r");
        QTRY_VERIFY_WITH_TIMEOUT(([&]
        {
            char bytes[128];
            const auto count = ::read(consoleMaster, bytes, sizeof(bytes));
            if (count > 0)
                received.append(bytes, count);
            return received.endsWith(quickCommands);
        }()), 2000);
        QVERIFY(wave->sessionEvents().constLast().message.contains("[UART] pid"));

        sensor->connectionChanged(false);
        QVERIFY(!wave->isRecording());
        QVERIFY(console->isOpen());
        QVERIFY(QMetaObject::invokeMethod(console, "onOpenClicked"));
        QVERIFY(!setSpeed->isEnabled());
        QVERIFY(!readSpeed->isEnabled());
        QVERIFY(!setPwm->isEnabled());
        QVERIFY(!setPid->isEnabled());
        QVERIFY(!readPid->isEnabled());
        const auto path = wave->bufferFileName();
        w.close();
        SampleArchive archive;
        QVERIFY(archive.open(path));
        QCOMPARE(archive.count(), 3);
#else
        QSKIP("伪终端测试使用 Linux PTY。");
#endif
    }
    void uartTerminalRendersFragmentedUtf8AndAnsi()
    {
        UartTerminal terminal;
        auto *view = terminal.findChild<QPlainTextEdit *>("uartTerminalView");
        terminal.setConnected(true);
        terminal.receive("\x1b[3");
        const auto utf8 = QStringLiteral("中文").toUtf8();
        terminal.receive("2m" + utf8.left(2));
        QVERIFY(view->toPlainText().isEmpty());
        terminal.receive(utf8.mid(2) + "\x1b[0m\r\nprogress 100%");
        QCOMPARE(view->toPlainText(), QStringLiteral("中文\nprogress 100%"));
        QTextCursor cursor(view->document());
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        QCOMPARE(cursor.charFormat().foreground().color(), QColor(0x98c379));
        terminal.receive("\r10%\x1b[K\r\nabc\bZ");
        QCOMPARE(view->toPlainText(), QStringLiteral("中文\n10%\nabZ"));
        terminal.receive("\x1b]0;ignored title\a\x1b[2J> ");
        QCOMPARE(view->toPlainText(), QString("> "));

        QSignalSpy submitted(&terminal, &UartTerminal::inputReady);
        terminal.receive("\x1b[?1h");
        QTest::keyClick(view, Qt::Key_Up);
        terminal.receive("\x1b[?1l");
        QTest::keyClick(view, Qt::Key_Up);
        QCOMPARE(submitted.count(), 2);
        QCOMPARE(submitted[0][0].toByteArray(), QByteArray("\x1bOA"));
        QCOMPARE(submitted[1][0].toByteArray(), QByteArray("\x1b[A"));
        QTest::keyClick(view, Qt::Key_Left);
        QTest::keyClick(view, Qt::Key_Right);
        QCOMPARE(submitted[2][0].toByteArray(), QByteArray("\x1b[D"));
        QCOMPARE(submitted[3][0].toByteArray(), QByteArray("\x1b[C"));
        terminal.receive("\r\x1b[K> ab\x1b" "7");
        terminal.receive("c\x1b" "8");
        terminal.receive("\bZ");
        QCOMPARE(view->toPlainText(), QString("> aZc"));
        terminal.setConnected(false);
        QTest::keyClick(view, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(submitted.count(), 4);
        QVERIFY(view->toPlainText().startsWith("> "));
        terminal.setConnected(true);
        QVERIFY(view->toPlainText().isEmpty());
    }
    void completedExportStillBlocksUntilResultHandled()
    {
        WavePanel panel;
        panel.startRecording();
        panel.addData(0, {1, 2, 3, 4});
        QSignalSpy spy(&panel, &WavePanel::exportFinished);
        const auto first = m_dir.filePath("first.csv");
        const auto second = m_dir.filePath("second.csv");
        QVERIFY(panel.exportDataTo(first));
        // 让工作线程完成，但先不处理 GUI 的 finished 事件。
        QTest::qSleep(100);
        QVERIFY(!panel.exportDataTo(second));
        QTRY_COMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toBool());
        QCOMPARE(spy.at(0).at(1).toString(), first);
        QVERIFY(panel.exportDataTo(second));
        QTRY_COMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(1).toString(), second);
    }
    void commandHexValidationAndUtf8()
    {
        CommandBar command;
        auto *input = command.findChild<QLineEdit *>("commandInput");
        auto *format = command.findChild<QComboBox *>("sendFormatCombo");
        auto *ending = command.findChild<QComboBox *>("sendTerminatorCombo");
        auto *send = command.findChild<QPushButton *>("sendCommandButton");
        QSignalSpy spy(&command, &CommandBar::commandSubmitted);
        input->setText("offline");
        QVERIFY(!send->isEnabled());
        QMetaObject::invokeMethod(&command, "submit");
        QCOMPARE(spy.count(), 0);
        command.setConnected(true);
        input->setText(QString::fromUtf8("转速"));
        ending->setCurrentIndex(1);
        QMetaObject::invokeMethod(&command, "submit");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toByteArray(), QString::fromUtf8("转速\n").toUtf8());
        command.acceptSubmission();
        format->setCurrentIndex(1);
        QVERIFY(!ending->isEnabled());
        for (const auto &invalid : {QStringLiteral("A"), QStringLiteral("GG"),
                                    QStringLiteral("0x01"), QStringLiteral("   ")})
        {
            input->setText(invalid);
            QVERIFY(!send->isEnabled());
            QMetaObject::invokeMethod(&command, "submit");
            QCOMPARE(spy.count(), 1);
        }
        input->setText("AA 55 00\tff");
        QVERIFY(send->isEnabled());
        QMetaObject::invokeMethod(&command, "submit");
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(1).at(0).toByteArray(), QByteArray::fromHex("aa5500ff"));
        command.findChild<QCheckBox *>("keepCommandCheck")->setChecked(true);
        command.acceptSubmission();
        QCOMPARE(input->text(), QString("AA 55 00\tff"));
    }
    void presetsRestoreFormatWithoutSending()
    {
        CommandBar command;
        auto *input = command.findChild<QLineEdit *>("commandInput");
        command.findChild<QComboBox *>("sendFormatCombo")->setCurrentIndex(1);
        input->setText("AA 01");
        QVERIFY(command.savePreset(QString::fromUtf8("查询状态")));
        input->setText("AA 02");
        QVERIFY(command.savePreset(QString::fromUtf8("查询状态")));
        CommandBar restored;
        QSignalSpy spy(&restored, &CommandBar::commandSubmitted);
        auto *presets = restored.findChild<QComboBox *>("commandPresets");
        QCOMPARE(presets->count(), 2);
        presets->setCurrentIndex(1);
        QCOMPARE(spy.count(), 0);
        QVERIFY(restored.isHexMode());
        QCOMPARE(restored.findChild<QLineEdit *>("commandInput")->text(), QString("AA 02"));
        restored.setConnected(true);
        QMetaObject::invokeMethod(&restored, "submit");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toByteArray(), QByteArray::fromHex("AA02"));
    }
    void commandHistoryRestoresModeAndDraft()
    {
        CommandBar command;
        command.setConnected(true);
        auto *input = command.findChild<QLineEdit *>("commandInput");
        auto *format = command.findChild<QComboBox *>("sendFormatCombo");
        auto *ending = command.findChild<QComboBox *>("sendTerminatorCombo");
        input->setText("status");
        ending->setCurrentIndex(1);
        command.acceptSubmission();
        format->setCurrentIndex(1);
        input->setText("FF 01");
        command.acceptSubmission();
        format->setCurrentIndex(0);
        ending->setCurrentIndex(3);
        input->setText("draft");
        QTest::keyClick(input, Qt::Key_Up);
        QCOMPARE(input->text(), QString("FF 01"));
        QVERIFY(command.isHexMode());
        QTest::keyClick(input, Qt::Key_Up);
        QCOMPARE(input->text(), QString("status"));
        QVERIFY(!command.isHexMode());
        QCOMPARE(ending->currentIndex(), 1);
        QTest::keyClick(input, Qt::Key_Down);
        QVERIFY(command.isHexMode());
        QTest::keyClick(input, Qt::Key_Down);
        QCOMPARE(input->text(), QString("draft"));
        QCOMPARE(ending->currentIndex(), 3);
        QVERIFY(!command.isHexMode());
    }
    void logFilteringFreezingAndExport()
    {
        LogPanel log;
        auto *view = log.findChild<QPlainTextEdit *>("logView");
        auto *search = log.findChild<QLineEdit *>("logSearch");
        auto *level = log.findChild<QComboBox *>("logLevelFilter");
        auto *channel = log.findChild<QComboBox *>("logChannelFilter");
        log.appendEntry(LogPanel::Level::Info, LogPanel::Channel::Receive, "alpha");
        log.appendEntry(LogPanel::Level::Error, LogPanel::Channel::System, "disk failed");
        log.appendEntry(LogPanel::Level::Warning, LogPanel::Channel::Transmit, "timeout");
        QTRY_COMPARE(view->document()->blockCount(), 3);
        search->setText("ALPHA");
        QTRY_VERIFY(view->toPlainText().contains("alpha") &&
                    !view->toPlainText().contains("disk failed"));
        search->clear();
        level->setCurrentIndex(1);
        QTRY_VERIFY(view->toPlainText().contains("disk failed") &&
                    !view->toPlainText().contains("alpha"));
        channel->setCurrentIndex(3);
        QTRY_VERIFY(!view->toPlainText().contains("timeout"));
        const auto filtered = m_dir.filePath("filtered-log.txt"),
                   all = m_dir.filePath("all-log.txt");
        QVERIFY(log.saveLogTo(filtered));
        QVERIFY(log.saveLogTo(all, false));
        QFile saved(filtered);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const auto text = saved.readAll();
        QVERIFY(text.contains("disk failed"));
        QVERIFY(!text.contains("alpha"));
        QFile full(all);
        QVERIFY(full.open(QIODevice::ReadOnly));
        QVERIFY(full.readAll().contains("alpha"));
        level->setCurrentIndex(0);
        channel->setCurrentIndex(0);
        QTRY_COMPARE(view->document()->blockCount(), 3);
        auto *freeze = log.findChild<QPushButton *>("freezeLogButton");
        freeze->setChecked(true);
        QTest::qWait(100);
        const auto before = view->toPlainText();
        log.append(LogPanel::Level::Info, "arrived while frozen");
        QTest::qWait(100);
        QCOMPARE(view->toPlainText(), before);
        const auto frozen = m_dir.filePath("frozen-log.txt"),
                   latest = m_dir.filePath("latest-log.txt");
        QVERIFY(log.saveLogTo(frozen));
        QVERIFY(log.saveLogTo(latest, false));
        QFile frozenFile(frozen), latestFile(latest);
        QVERIFY(frozenFile.open(QIODevice::ReadOnly));
        QVERIFY(latestFile.open(QIODevice::ReadOnly));
        QVERIFY(!frozenFile.readAll().contains("arrived while frozen"));
        QVERIFY(latestFile.readAll().contains("arrived while frozen"));
        freeze->setChecked(false);
        QTRY_VERIFY(view->toPlainText().contains("arrived while frozen"));
    }
    void logRetentionAndFrozenSnapshot()
    {
        LogPanel log;
        auto *view = log.findChild<QPlainTextEdit *>("logView");
        for (int i = 0; i < 2000; ++i)
            log.append(LogPanel::Level::Info,
                       QStringLiteral("entry%1").arg(i, 4, 10, QLatin1Char('0')));
        QTRY_COMPARE(view->document()->blockCount(), 2000);
        QVERIFY(view->toPlainText().contains("entry0000"));
        auto *freeze = log.findChild<QPushButton *>("freezeLogButton");
        freeze->setChecked(true);
        for (int i = 2000; i < 2050; ++i)
            log.append(LogPanel::Level::Info, QStringLiteral("entry%1").arg(i));
        QTest::qWait(100);
        QVERIFY(view->toPlainText().contains("entry0000"));
        QVERIFY(!view->toPlainText().contains("entry2049"));
        freeze->setChecked(false);
        QTRY_VERIFY(view->toPlainText().contains("entry2049"));
        QVERIFY(!view->toPlainText().contains("entry0000"));
        QCOMPARE(view->document()->blockCount(), 2000);
        for (int i = 2050; i < 2100; ++i)
            log.append(LogPanel::Level::Info, QStringLiteral("entry%1").arg(i));
        QTRY_VERIFY(view->toPlainText().contains("entry2099"));
        QVERIFY(!view->toPlainText().contains("entry0050"));
        QCOMPARE(view->document()->blockCount(), 2000);
    }
    void rawHexLoggingDoesNotLimitAcquisition()
    {
        MainWindow w;
        auto *sensor = w.findChild<CanPanel *>("sensorCanPanel");
        sensor->connectionChanged(true);
        auto *console = w.findChild<SerialPanel *>("consoleUartPanel");
        auto *wave = w.findChild<WavePanel *>();
        auto *log = w.findChild<LogPanel *>();
        auto *mode = log->findChild<QComboBox *>("receiveLogMode");
        auto *view = log->findChild<QPlainTextEdit *>("logView");
        mode->setCurrentIndex(1);
        QByteArray batch;
        for (int i = 0; i < 1000; ++i)
            batch += frame();
        for (int i = 0; i < 1000; ++i)
            sensor->frameReceived(0x100, canQualityFrame());
        console->dataReceived(batch);
        QCOMPARE(wave->sampleCount(), 1000);
        QTRY_VERIFY(view->toPlainText().contains("FF CD CC CC 3D"));
        QTRY_VERIFY(view->toPlainText().contains(QString::fromUtf8("显示限速")));
        mode->setCurrentIndex(2);
        QTest::qWait(100);
        const auto before = view->toPlainText();
        sensor->frameReceived(0x100, canQualityFrame());
        console->dataReceived(frame());
        QCOMPARE(wave->sampleCount(), 1001);
        QTest::qWait(180);
        QCOMPARE(view->toPlainText(), before);
        mode->setCurrentIndex(0);
        console->dataReceived("split ");
        console->dataReceived("text\n");
        QTRY_VERIFY(view->toPlainText().contains("split text"));
        const auto utf8 = QString::fromUtf8("中文").toUtf8();
        console->dataReceived(utf8.left(1));
        QTest::qWait(240);
        QVERIFY(!view->toPlainText().contains(QChar(0xfffd)));
        console->dataReceived(utf8.mid(1));
        QTRY_VERIFY(view->toPlainText().contains(QString::fromUtf8("中文")));
        console->dataReceived("save pending text");
        const auto logPath = m_dir.filePath("pending-log.txt");
        QVERIFY(log->saveLogTo(logPath));
        QFile output(logPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QVERIFY(output.readAll().contains("save pending text"));
        w.close();
    }
    void deviceIdentityAndHotplugSelection()
    {
        const SerialDevice target{"ttyUSB0", "USB UART", "unique-A", 0x1234, 0x5678};
        const SerialDevice moved{"ttyUSB2", "USB UART", "unique-A", 0x1234, 0x5678};
        const SerialDevice other{"ttyUSB0", "USB UART", "unique-B", 0x1234, 0x5678};
        QCOMPARE(SerialPanel::matchingPort({other, moved}, target), QString("ttyUSB2"));
        QVERIFY(SerialPanel::matchingPort({other}, target).isEmpty());
        auto duplicate = moved;
        duplicate.port = "ttyUSB3";
        QVERIFY(SerialPanel::matchingPort({moved, duplicate}, target).isEmpty());
        duplicate.serialNumber.clear();
        auto noSerial = target;
        noSerial.serialNumber.clear();
        QVERIFY(SerialPanel::matchingPort({duplicate}, noSerial).isEmpty());
        duplicate.port = target.port;
        QCOMPARE(SerialPanel::matchingPort({duplicate}, noSerial), target.port);
        duplicate.vendor = 0xffff;
        QVERIFY(SerialPanel::matchingPort({duplicate}, noSerial).isEmpty());

        QSettings().remove("serial/port");
        QSettings().remove("serial/deviceSerial");
        QList<SerialDevice> inventory{target, moved};
        SerialPanel panel(nullptr, [&] { return inventory; });
        auto *ports = panel.findChild<QComboBox *>("serialPortCombo");
        QCOMPARE(ports->currentData().toString(), target.port);
        inventory = {moved};
        QVERIFY(QMetaObject::invokeMethod(&panel, "pollPorts"));
        QCOMPARE(ports->count(), 1);
        QCOMPARE(ports->currentIndex(), -1); // 消失后不默认切到另一块板子
        inventory = {target, other};
        QVERIFY(QMetaObject::invokeMethod(&panel, "pollPorts"));
        QCOMPARE(ports->count(), 2);
        QVERIFY(!panel.isOpen()); // 自动刷新不代表自动连接
    }
    void reconnectUsesOriginalDeviceAndCanBeCancelled()
    {
#ifdef Q_OS_LINUX
        const int firstMaster = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        const int secondMaster = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        QVERIFY(firstMaster >= 0 && secondMaster >= 0);
        auto cleanup = qScopeGuard([&] { ::close(firstMaster); ::close(secondMaster); });
        QCOMPARE(grantpt(firstMaster), 0);
        QCOMPARE(unlockpt(firstMaster), 0);
        QCOMPARE(grantpt(secondMaster), 0);
        QCOMPARE(unlockpt(secondMaster), 0);
        const auto firstPort = QString::fromLocal8Bit(ptsname(firstMaster));
        const auto secondPort = QString::fromLocal8Bit(ptsname(secondMaster));
        QList<SerialDevice> inventory{{firstPort, "Board", "board-A", 1, 2}};
        SerialPanel panel(nullptr, [&] { return inventory; });
        auto *combo = panel.findChild<QComboBox *>("serialPortCombo");
        combo->setCurrentIndex(0);
        auto *automatic = panel.findChild<QCheckBox *>("autoReconnectCheck");
        automatic->setChecked(true);
        QSignalSpy connections(&panel, &SerialPanel::connectionChanged);
        QVERIFY(QMetaObject::invokeMethod(&panel, "onOpenClicked"));
        QVERIFY(panel.isOpen());
        // 使用真实 QSerialPort，注入资源消失通知与可枚举设备列表。
        QVERIFY(QMetaObject::invokeMethod(&panel, "onErrorOccurred",
            Q_ARG(QSerialPort::SerialPortError, QSerialPort::ResourceError)));
        QVERIFY(!panel.isOpen());
        QVERIFY(panel.isWaitingForReconnect());
        inventory = {{secondPort, "Other board", "board-B", 1, 2}};
        QVERIFY(QMetaObject::invokeMethod(&panel, "pollPorts"));
        QVERIFY(!panel.isOpen());
        inventory = {{secondPort, "Board", "board-A", 1, 2}};
        QVERIFY(QMetaObject::invokeMethod(&panel, "pollPorts"));
        QVERIFY(panel.isOpen());
        QVERIFY(!panel.isWaitingForReconnect());
        QCOMPARE(connections.count(), 3);
        QCOMPARE(combo->currentData().toString(), secondPort);
        const QByteArray command("status\n");
        QCOMPARE(panel.sendData(command), qint64(command.size()));
        QByteArray received;
        auto readCommand = [&]
        {
            char bytes[128];
            const auto count = ::read(secondMaster, bytes, sizeof(bytes));
            if (count > 0) received.append(bytes, count);
            return received == command;
        };
        QTRY_VERIFY(readCommand());
        QVERIFY(QMetaObject::invokeMethod(&panel, "onOpenClicked"));
        QVERIFY(!panel.isWaitingForReconnect()); // 手动断开不重连
        QVERIFY(QMetaObject::invokeMethod(&panel, "pollPorts"));
        QVERIFY(!panel.isOpen());
        QVERIFY(QMetaObject::invokeMethod(&panel, "onOpenClicked"));
        QVERIFY(panel.isOpen());
        QVERIFY(QMetaObject::invokeMethod(&panel, "onErrorOccurred",
            Q_ARG(QSerialPort::SerialPortError, QSerialPort::ResourceError)));
        QVERIFY(panel.isWaitingForReconnect());
        QVERIFY(QMetaObject::invokeMethod(&panel, "onOpenClicked"));
        QVERIFY(!panel.isWaitingForReconnect());
        QVERIFY(QMetaObject::invokeMethod(&panel, "pollPorts"));
        QVERIFY(!panel.isOpen());
        QVERIFY(QMetaObject::invokeMethod(&panel, "onOpenClicked"));
        QVERIFY(QMetaObject::invokeMethod(&panel, "onErrorOccurred",
            Q_ARG(QSerialPort::SerialPortError, QSerialPort::ResourceError)));
        automatic->setChecked(false);
        QVERIFY(!panel.isWaitingForReconnect());
#else
        QSKIP("重连串口测试使用 Linux PTY。");
#endif
    }
    void channelSettingsPersistAndDoNotFilterRecording()
    {
        {
            WavePanel panel;
            panel.startRecording();
            panel.addData(0, {5, 10000, 100, 200});
            panel.addData(1, {6, 11000, 150, 200});
            auto *quality = panel.findChild<QCustomPlot *>("qualityPlot");
            panel.setChannelVisible(1, false);
            panel.setChannelColor(0, QColor("#abcdef"));
            QVERIFY(QMetaObject::invokeMethod(&panel, "onReplotTimeout"));
            QVERIFY(quality->yAxis->range().upper < 20); // 隐藏极大值不参与自动缩放
            QVERIFY(!panel.setFixedYRange(0, 10, 5));
            QVERIFY(!panel.setFixedYRange(3, 0, 1));
            QVERIFY(!panel.setFixedYRange(0, qQNaN(), 10));
            QVERIFY(panel.setFixedYRange(0, 0, 100));
            QVERIFY(panel.setFixedYRange(1, 0, 3000));
            panel.addData(2, {7, 12000, 170, 200});
            QVERIFY(QMetaObject::invokeMethod(&panel, "onReplotTimeout"));
            QCOMPARE(quality->yAxis->range().lower, 0.0);
            QCOMPARE(quality->yAxis->range().upper, 100.0);
            QCOMPARE(panel.sampleCount(), 3);
            QVERIFY(panel.stopRecording());
            SampleArchive archive;
            QVERIFY(archive.open(panel.bufferFileName()));
            QVector<double> impurity;
            QVERIFY(archive.forEachSample([&](double, const double *v)
                { impurity.append(v[1]); return true; }));
            QCOMPARE(impurity, QVector<double>({10000, 11000, 12000}));
        }
        WavePanel restored;
        auto *quality = restored.findChild<QCustomPlot *>("qualityPlot");
        QVERIFY(!quality->graph(1)->visible());
        QCOMPARE(quality->graph(0)->pen().color(), QColor("#abcdef"));
        QCOMPARE(quality->yAxis->range().upper, 100.0);
        QCOMPARE(quality->yAxis->range().lower, 0.0);
        restored.setChannelVisible(1, true);
        QVERIFY(quality->graph(1)->visible());
    }
    void eventPersistenceRecoveryAndSaveAs()
    {
        const auto session = m_dir.filePath("events.swave");
        const auto start = QDateTime::currentDateTimeUtc();
        SessionEvents events;
        events.begin(session, start);
        QVERIFY(events.append(0, "record_start", "开始"));
        QVERIFY(events.append(1.25, "command", "rpm 1500\r\n"));
        QVERIFY(events.append(2, "note", "实验一\n第二行"));
        QVERIFY(!events.hasPending());
        SessionEvents loaded;
        QVERIFY(loaded.load(session, start));
        QCOMPARE(loaded.entries().size(), 3);
        QCOMPARE(loaded.entries()[1].time, 1.25);
        QCOMPARE(loaded.entries()[2].message, QString::fromUtf8("实验一\n第二行"));
        QVERIFY(!loaded.append(3, "note", "readonly"));
        QFile file(SessionEvents::fileName(session));
        QVERIFY(file.open(QIODevice::Append));
        QVERIFY(file.write("{\"time_s\":3") > 0);
        file.close();
        QVERIFY(loaded.load(session, start));
        QVERIFY(loaded.ignoredTail());
        QCOMPARE(loaded.entries().size(), 3);
        QVERIFY(!loaded.load(session, start.addSecs(1))); // 同名文件不能串会话
        QCOMPARE(loaded.entries().size(), 0);

        const auto destination = m_dir.filePath("event-copy.swave");
        QVERIFY(events.relocate(destination));
        QVERIFY(loaded.load(destination, start));
        QCOMPARE(loaded.entries().size(), 3);
        QVERIFY(events.append(3, "disconnect", "拔出"));
        QVERIFY(loaded.load(destination, start));
        QCOMPARE(loaded.entries().size(), 4);
        QVERIFY(loaded.load(m_dir.filePath("legacy-no-events.swave"), start));
        QCOMPARE(loaded.entries().size(), 0);
    }
    void eventWriteFailureCanBeRecovered()
    {
        const auto start = QDateTime::currentDateTimeUtc();
        const auto session = m_dir.filePath("event-failure.swave");
        SessionEvents events;
        events.begin(session, start);
        QVERIFY(events.append(0, "note", "first"));
        // 文件被外部移走：追加失败必须保留内存里的事件，供另存恢复。
        const auto backup = SessionEvents::fileName(session) + ".moved";
        QVERIFY(QFile::rename(SessionEvents::fileName(session), backup));
        QVERIFY(!events.append(1, "note", "pending"));
        QVERIFY(events.hasPending());
        QCOMPARE(events.entries().size(), 2);
        const auto recovered = m_dir.filePath("event-recovery.swave");
        QVERIFY(events.relocate(recovered));
        QVERIFY(!events.hasPending());
        SessionEvents loaded;
        QVERIFY(loaded.load(recovered, start));
        QCOMPARE(loaded.entries().size(), 2);
        QCOMPARE(loaded.entries().back().message, QString("pending"));
        QFile original(backup);
        QVERIFY(original.open(QIODevice::ReadOnly));
        QVERIFY(!original.readAll().contains("pending"));
    }
    void partialEventWriteIsRetriedWithoutDuplicates()
    {
#ifdef Q_OS_LINUX
        const auto session = m_dir.filePath("partial-event.swave");
        const auto start = QDateTime::currentDateTimeUtc();
        SessionEvents events;
        events.begin(session, start);
        QVERIFY(events.append(0, "note", "first"));
        struct rlimit originalLimit;
        QCOMPARE(getrlimit(RLIMIT_FSIZE, &originalLimit), 0);
        const auto oldHandler = ::signal(SIGXFSZ, SIG_IGN);
        {
            auto restore = qScopeGuard([&]
            {
                setrlimit(RLIMIT_FSIZE, &originalLimit);
                ::signal(SIGXFSZ, oldHandler);
            });
            struct rlimit limit = originalLimit;
            limit.rlim_cur = QFileInfo(SessionEvents::fileName(session)).size() + 30;
            QCOMPARE(setrlimit(RLIMIT_FSIZE, &limit), 0);
            QVERIFY(!events.append(1, "note", QString(1024, 'x')));
            QVERIFY(events.hasPending());
        }
        QVERIFY(events.append(2, "note", "after retry"));
        QVERIFY(!events.hasPending());
        SessionEvents loaded;
        QVERIFY(loaded.load(session, start));
        QCOMPARE(loaded.entries().size(), 3);
        QCOMPARE(loaded.entries()[1].message, QString(1024, 'x'));
        QCOMPARE(loaded.entries()[2].message, QString("after retry"));
#else
        QSKIP("部分写入测试使用 Linux 文件大小限制。");
#endif
    }
    void waveformSaveAsKeepsEventsAfterPartialFailure()
    {
        WavePanel panel;
        panel.startRecording();
        panel.addData(0, {1, 2, 3, 4});
        QVERIFY(panel.addSessionEvent("note", "must keep"));
        const auto original = panel.bufferFileName();
        const auto partial = m_dir.filePath("partial-copy.swave");
        QVERIFY(QDir().mkpath(SessionEvents::fileName(partial))); // 阻止写入配套文件
        QSignalSpy failures(&panel, &WavePanel::storageFailed);
        QVERIFY(!panel.saveSessionAs(partial));
        QCOMPARE(panel.bufferFileName(), partial); // 波形文件已成功切换
        QVERIFY(panel.hasStorageError());
        QVERIFY(!panel.isRecording());
        QCOMPARE(failures.count(), 1);
        QCOMPARE(panel.sessionEvents().size(), 2);
        QVERIFY(!panel.flushRecording());
        const auto recovered = m_dir.filePath("whole-recovery.swave");
        QVERIFY(panel.saveSessionAs(recovered));
        QVERIFY(!panel.hasStorageError());
        SessionEvents events;
        QVERIFY(events.load(recovered, panel.startedAt()));
        QCOMPARE(events.entries().size(), 2);
        QCOMPARE(events.entries().back().message, QString("must keep"));
        SampleArchive archive;
        QVERIFY(archive.open(recovered));
        QCOMPARE(archive.count(), 1);
        QVERIFY(QFile::exists(original));
        QVERIFY(QFile::exists(SessionEvents::fileName(original)));
    }
    void channelDialogValidationCancelAndDefaults()
    {
        WavePanel panel;
        panel.resize(1200, 750);
        panel.show();
        auto *plot = panel.findChild<QCustomPlot *>("qualityPlot");
        bool invalidKeptOpen = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto *automatic = dialog->findChild<QCheckBox *>("autoYAxisCheck");
            automatic->setChecked(false);
            dialog->findChild<QDoubleSpinBox *>("plot0Limit0")->setValue(100);
            dialog->findChild<QDoubleSpinBox *>("plot0Limit1")->setValue(50);
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            buttons->button(QDialogButtonBox::Ok)->click();
            invalidKeptOpen = dialog->isVisible();
            dialog->findChild<QCheckBox *>("channel0VisibleCheck")->setChecked(false);
            buttons->button(QDialogButtonBox::Cancel)->click();
        });
        panel.configureChannels();
        QVERIFY(invalidKeptOpen);
        QVERIFY(plot->graph(0)->visible()); // 取消不应用任何修改
        panel.setChannelVisible(0, false);
        panel.setChannelColor(0, QColor("#abcdef"));
        QVERIFY(panel.setFixedYRange(0, 0, 100));
        QTimer::singleShot(0, &panel, [&]
        {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            auto *buttons = dialog->findChild<QDialogButtonBox *>();
            buttons->button(QDialogButtonBox::RestoreDefaults)->click();
            const auto screenshot = qEnvironmentVariable("SERIAL_WAVE_SCREENSHOT");
            if (!screenshot.isEmpty()) dialog->grab().save(screenshot + ".channels.png");
            buttons->button(QDialogButtonBox::Ok)->click();
        });
        panel.configureChannels();
        QVERIFY(plot->graph(0)->visible());
        QCOMPARE(plot->graph(0)->pen().color(), QColor(70, 220, 150));
        QCOMPARE(plot->yAxis->range().upper, 20.0);
    }
    void sessionEventsFollowOperationsAndReplay()
    {
        MainWindow window;
        auto *sensor = window.findChild<CanPanel *>("sensorCanPanel");
        auto *wave = window.findChild<WavePanel *>();
        sensor->connectionChanged(true);
        sensor->frameReceived(0x100, canQualityFrame());
        QTest::qWait(10);
        QVERIFY(wave->addSessionEvent("note", "第一次实验"));
        const auto noteTime = wave->sessionEvents().back().time;
        sensor->frameReceived(0x100, canQualityFrame());
        QVERIFY(wave->lastTime() >= noteTime); // 事件与解析器使用同一个单调时钟起点
        QVERIFY(wave->lastTime() - noteTime < .05);
        sensor->connectionChanged(false);
        QCOMPARE(wave->sessionEvents().size(), 5);
        const auto original = wave->bufferFileName();
        const auto copy = m_dir.filePath("gui-events-copy.swave");
        QVERIFY(wave->saveSessionAs(copy));
        QVERIFY(QFile::exists(SessionEvents::fileName(copy)));
        QVERIFY(QFile::exists(SessionEvents::fileName(original)));
        SessionEvents events;
        QVERIFY(events.load(copy, wave->startedAt()));
        QCOMPARE(events.entries().size(), 5);
        QVERIFY(wave->openSession(copy));
        QCOMPARE(wave->sessionEvents().size(), 5);
        QVERIFY(!wave->addSessionEvent("note", "readonly"));
        QVERIFY(!wave->findChild<QPushButton *>("addSessionNoteButton")->isEnabled());
        wave->showEvents();
        auto *table = wave->findChild<QTableView *>("sessionEventsTable");
        QVERIFY(table);
        QCOMPARE(table->model()->rowCount(), 5);
        const auto screenshot = qEnvironmentVariable("SERIAL_WAVE_SCREENSHOT");
        if (!screenshot.isEmpty()) QVERIFY(table->window()->grab().save(screenshot + ".events.png"));
        QVERIFY(QMetaObject::invokeMethod(table, "doubleClicked",
            Q_ARG(QModelIndex, table->model()->index(2, 0))));
        QTRY_VERIFY(wave->findChild<QCustomPlot *>("qualityPlot")->graph(0)->dataCount() > 0);
        QVERIFY(QMetaObject::invokeMethod(wave, "onReplotTimeout"));
        int visibleMarkers = 0;
        auto *plot = wave->findChild<QCustomPlot *>("qualityPlot");
        for (int i = 0; i < plot->itemCount(); ++i)
            if (dynamic_cast<QCPItemLine *>(plot->item(i)) && plot->item(i)->visible())
                ++visibleMarkers;
        QVERIFY(visibleMarkers >= 5);
        wave->clearData();
        QCOMPARE(table->model()->rowCount(), 1); // 新会话只保留自己的开始事件
        QCOMPARE(wave->sessionEvents().size(), 1);
        window.close();
    }
    void actuatorFramesSupportFragmentationAndLegacyFallback()
    {
        const auto packet = actuatorFrame(42.5, ActuatorState::Retracting);
        QCOMPARE(packet.size(), 23);
        for (int cut = 1; cut < packet.size(); ++cut)
        {
            ProtocolParser parser;
            TelemetrySample sample;
            parser.setFrameHandler([&](const auto &value) { sample = value; });
            parser.feed(packet.left(cut));
            QCOMPARE(parser.frameCount(), 0);
            parser.feed(packet.mid(cut));
            QCOMPARE(parser.frameCount(), 1);
            QCOMPARE(sample.actuatorPosition, 42.5);
            QCOMPARE(sample.actuatorState, ActuatorState::Retracting);
            QVERIFY(qAbs(sample.crushRate - 10) < 1e-5);
        }
        ProtocolParser parser;
        QVector<TelemetrySample> samples;
        MachineStatus status;
        parser.setFrameHandler([&](const auto &sample) { samples.append(sample); });
        parser.setStatusHandler([&](const auto &s) { status = s; });
        parser.feed(frame() + packet + frame());
        QCOMPARE(samples.size(), 3);
        QVERIFY(qIsNaN(samples[0].actuatorPosition));
        QCOMPARE(samples[1].values()[4], 42.5);
        QCOMPARE(samples[1].values()[5], double(ActuatorState::Retracting));
        QVERIFY(qIsNaN(samples[2].actuatorPosition));
        QVERIFY(qIsNaN(status.actuatorPosition)); // 旧帧不能冒充最新的推杆反馈
        parser.feed(actuatorFrame(0, ActuatorState::FullyRetracted));
        QCOMPARE(status.actuatorPosition, 0.0);
        parser.feed(actuatorFrame(100, ActuatorState::FullyExtended));
        QCOMPARE(status.actuatorPosition, 100.0);
        const auto count = parser.frameCount();
        parser.feed(actuatorFrame(-1) + actuatorFrame(101) + actuatorFrame(qQNaN()) +
                    actuatorFrame(50, ActuatorState(99)));
        QCOMPARE(parser.frameCount(), count);
        QVERIFY(parser.errorCount() >= 4);
        parser.feed(actuatorFrame(30, ActuatorState::Fault));
        QCOMPARE(parser.frameCount(), count + 1);
        QCOMPARE(status.actuatorState, ActuatorState::Fault);
        parser.resetSession();
        parser.feed(frame());
        QVERIFY(qIsNaN(status.actuatorPosition));
    }
    void actuatorStateAndMissingDataAreDistinct()
    {
        StatusPanel panel;
        panel.setConnected(true);
        auto *position = panel.findChild<QLabel *>("actuatorPositionValue");
        auto *state = panel.findChild<QLabel *>("actuatorStateValue");
        auto *progress = panel.findChild<QProgressBar *>("actuatorProgress");
        QCOMPARE(state->text(), QString::fromUtf8("暂无数据"));
        MachineStatus status;
        status.rpm = 1500;
        status.running = true;
        status.actuatorPosition = 42.5;
        status.actuatorState = ActuatorState::Extending;
        panel.updateStatus(status);
        QCOMPARE(position->text(), QString("42.5 %"));
        QCOMPARE(progress->value(), 425);
        QCOMPARE(state->text(), QString::fromUtf8("伸出中"));
        status.actuatorState = ActuatorState::Retracting;
        panel.updateStatus(status);
        QCOMPARE(state->text(), QString::fromUtf8("缩回中"));
        status.actuatorState = ActuatorState::Fault;
        panel.updateStatus(status);
        QCOMPARE(state->text(), QString::fromUtf8("故障"));
        QTest::qWait(2050);
        panel.refreshFreshness();
        QCOMPARE(state->text(), QString::fromUtf8("数据过期"));
        QVERIFY(!progress->isEnabled());
        panel.setConnected(false);
        QCOMPARE(state->text(), QString::fromUtf8("未连接"));
        panel.setConnected(true);
        panel.updateStatus(MachineStatus{});
        QCOMPARE(position->text(), QString("--"));
        QCOMPARE(state->text(), QString::fromUtf8("暂无数据"));
        panel.setReplayMode(true);
        QCOMPARE(state->text(), QString::fromUtf8("历史回看"));
    }
    void actuatorArchiveExportAndHistoryRemainCompatible()
    {
        const auto directory = m_dir.filePath("actuator-archives");
        QVERIFY(QDir().mkpath(directory));
        const auto legacy = createTestSession(directory, "legacy");
        QVERIFY(!legacy.isEmpty());
        const auto current = createTestSession(directory, "extended", sampleChannelCount);
        QVERIFY(!current.isEmpty());
        SampleArchive archive;
        QVERIFY(archive.open(current));
        QCOMPARE(archive.channelCount(), sampleChannelCount);
        QVERIFY(archive.open(legacy));
        QCOMPARE(archive.channelCount(), 4);
        auto cancelled = std::make_shared<std::atomic_bool>(false);
        const auto oldView = readSessionView(legacy, 0, 10, cancelled);
        QCOMPARE(oldView.count, 1);
        QVERIFY(qIsNaN(oldView.points[0].values[4]));
        QCOMPARE(oldView.stats[4].count, 0);
        const auto newView = readSessionView(current, 0, 10, cancelled);
        QCOMPARE(newView.points[0].values[4], 42.5);
        QCOMPARE(newView.stats[4].count, 1);
        auto exportState = std::make_shared<ExportState>();
        exportState->total = 1;
        const auto json = QDir(directory).filePath("actuator.json");
        QVERIFY(exportSession(current, json, exportState).ok);
        QFile result(json);
        QVERIFY(result.open(QIODevice::ReadOnly));
        const auto object = QJsonDocument::fromJson(result.readAll()).object();
        QCOMPARE(object.value("version").toInt(), 2);
        const auto sample = object.value("samples").toArray()[0].toObject();
        QCOMPARE(sample.value("actuator_position_pct").toDouble(), 42.5);
        QCOMPARE(sample.value("actuator_state").toInt(), int(ActuatorState::Extending));

        WavePanel panel;
        QVERIFY(panel.openSession(legacy));
        auto *plot = panel.findChild<QCustomPlot *>("actuatorPlot");
        QVERIFY(plot);
        QCOMPARE(plot->graphCount(), 1);
        QTRY_VERIFY(plot->graph(0)->dataCount() > 0);
        QVERIFY(qIsNaN(plot->graph(0)->data()->constBegin()->value));
        QVERIFY(panel.openSession(current));
        QTRY_VERIFY(plot->graph(0)->dataCount() > 0 &&
                    qIsFinite(plot->graph(0)->data()->constBegin()->value));
        QCOMPARE(plot->graph(0)->data()->constBegin()->value, 42.5);
        QCOMPARE(plot->yAxis->range().lower, 0.0);
        QCOMPARE(plot->yAxis->range().upper, 100.0);
        plot->xAxis->setRange(-1, 1);
        QCOMPARE(plot->xAxis->range().lower, 0.0);
        QCOMPARE(panel.findChild<QCustomPlot *>("speedPlot")->xAxis->range().upper,
                 plot->xAxis->range().upper);
        panel.clearData();
        panel.addData(0, {1, 2, 3, 4, 65, double(ActuatorState::Stopped)});
        QVERIFY(panel.stopRecording());
        QVERIFY(archive.open(panel.bufferFileName()));
        QCOMPARE(archive.channelCount(), sampleChannelCount); // 回看旧文件后新记录仍用新格式
    }
    void canActuatorReceptionKeepsLatestFeedbackAcrossQualityFrames()
    {
        MainWindow window;
        auto *sensor = window.findChild<CanPanel *>("sensorCanPanel");
        auto *wave = window.findChild<WavePanel *>();
        auto *status = window.findChild<StatusPanel *>();
        sensor->connectionChanged(true);
        const auto payload = actuatorFrame(62.5, ActuatorState::Retracting).mid(17, 5);
        sensor->frameReceived(0x102, payload.left(4));
        QCOMPARE(wave->sampleCount(), 0);
        sensor->frameReceived(0x102, payload);
        QCOMPARE(wave->sampleCount(), 1);
        QCOMPARE(status->findChild<QLabel *>("actuatorPositionValue")->text(), QString("62.5 %"));
        QCOMPARE(status->findChild<QLabel *>("actuatorStateValue")->text(), QString::fromUtf8("缩回中"));
        auto *plot = wave->findChild<QCustomPlot *>("actuatorPlot");
        QCOMPARE(plot->graph(0)->data()->constBegin()->value, 62.5);
        sensor->frameReceived(0x100, canQualityFrame());
        QCOMPARE(wave->sampleCount(), 2);
        QCOMPARE(status->findChild<QLabel *>("actuatorStateValue")->text(), QString::fromUtf8("缩回中"));
        QCOMPARE(plot->graph(0)->dataCount(), 1);
        QVERIFY(wave->stopRecording());
        SampleArchive archive;
        QVERIFY(archive.open(wave->bufferFileName()));
        QVector<double> positions, states;
        QVERIFY(archive.forEachSample([&](double, const double *values)
        {
            positions << values[4]; states << values[5]; return true;
        }));
        QCOMPARE(positions.size(), 2);
        QCOMPARE(positions[0], 62.5);
        QCOMPARE(states[0], double(ActuatorState::Retracting));
        QVERIFY(qIsNaN(positions[1]));
        QVERIFY(qIsNaN(states[1]));
        window.close();
    }
    void missingActuatorSamplesDoNotPoisonStatistics()
    {
        const auto path = m_dir.filePath("actuator-missing.swave");
        SampleArchive archive(sampleChannelCount);
        QVERIFY(archive.create(path));
        double values[] = {1, 2, 3, 4, qQNaN(), qQNaN()};
        QVERIFY(archive.append(0, values));
        values[4] = 20; values[5] = 1;
        QVERIFY(archive.append(1, values));
        values[4] = 80; values[5] = 2;
        QVERIFY(archive.append(2, values));
        values[4] = qQNaN(); values[5] = qQNaN();
        QVERIFY(archive.append(3, values));
        QVERIFY(archive.flush());
        const auto view = readSessionView(path, 0, 10, std::make_shared<std::atomic_bool>(false));
        QCOMPARE(view.count, 4);
        QCOMPARE(view.stats[4].count, 2);
        QCOMPARE(view.stats[4].min, 20.0);
        QCOMPARE(view.stats[4].max, 80.0);
        QCOMPARE(view.stats[4].mean, 50.0);
        auto state = std::make_shared<ExportState>();
        state->total = 4;
        const auto json = m_dir.filePath("actuator-missing.json");
        QVERIFY(exportSession(path, json, state).ok);
        QFile file(json);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto samples = QJsonDocument::fromJson(file.readAll()).object().value("samples").toArray();
        QVERIFY(samples[0].toObject().value("actuator_position_pct").isNull());
        QCOMPARE(samples[1].toObject().value("actuator_position_pct").toDouble(), 20.0);
    }
    void cleanupOnlyDeletesConfirmedHistory()
    {
        const auto directory = m_dir.filePath("cleanup");
        QVERIFY(QDir().mkpath(directory));
        const auto active = createTestSession(directory, "active");
        const auto old = createTestSession(directory, "old");
        const auto changing = createTestSession(directory, "changing");
        QVERIFY(!active.isEmpty() && !old.isEmpty() && !changing.isEmpty());
        QFile unrelated(QDir(directory).filePath("export.csv"));
        QVERIFY(unrelated.open(QIODevice::WriteOnly));
        unrelated.write("keep"); unrelated.close();
        QFile unknown(QDir(directory).filePath("unknown.swave"));
        QVERIFY(unknown.open(QIODevice::WriteOnly)); unknown.write("not our archive"); unknown.close();
        const auto plan = planSessionCleanup(directory, {active});
        QCOMPARE(plan.sessions, 2);
        QCOMPARE(plan.protectedSessions, 1);
        QCOMPARE(plan.files.size(), 4);
        const auto createdLater = createTestSession(directory, "later");
        QFile modified(changing);
        QVERIFY(modified.open(QIODevice::Append)); modified.write("changed"); modified.close();
        const auto result = executeSessionCleanup(plan, {active});
        QCOMPARE(result.sessionsRemoved, 1);
        QCOMPARE(result.filesRemoved, 2);
        QVERIFY(!QFile::exists(old));
        QVERIFY(!QFile::exists(SessionEvents::fileName(old)));
        QVERIFY(QFile::exists(changing));
        QVERIFY(QFile::exists(SessionEvents::fileName(changing)));
        QVERIFY(QFile::exists(active));
        QVERIFY(QFile::exists(createdLater));
        QVERIFY(QFile::exists(unrelated.fileName()));
        QVERIFY(QFile::exists(unknown.fileName()));
        QVERIFY(!result.errors.isEmpty());
    }
    void cleanupRechecksProtectionAndSkipsSymlinks()
    {
        const auto directory = m_dir.filePath("cleanup-protection");
        const auto outside = m_dir.filePath("outside");
        QVERIFY(QDir().mkpath(directory)); QVERIFY(QDir().mkpath(outside));
        const auto session = createTestSession(directory, "newly-active");
        const auto foreign = createTestSession(outside, "foreign");
#ifdef Q_OS_LINUX
        const auto link = QDir(directory).filePath("link.swave");
        QVERIFY(QFile::link(foreign, link));
        QVERIFY(QFileInfo(link).isSymLink());
#endif
        const auto plan = planSessionCleanup(directory, {});
        QCOMPARE(plan.sessions, 1);
        const auto result = executeSessionCleanup(plan, {session});
        QCOMPARE(result.sessionsRemoved, 0);
        QVERIFY(QFile::exists(session));
        QVERIFY(QFile::exists(foreign));
    }
    void cleanupDialogRequiresExplicitConfirmation()
    {
        const auto directory = m_dir.filePath("cleanup-ui");
        QVERIFY(QDir().mkpath(directory));
        const auto previous = QSettings().value("recording/directory");
        QSettings().setValue("recording/directory", directory);
        auto restore = qScopeGuard([&] { QSettings().setValue("recording/directory", previous); });
        const auto old = createTestSession(directory, "old-ui");
        WavePanel panel;
        panel.startRecording();
        const auto active = panel.bufferFileName();
        bool cancellationWasDefault = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) return;
            cancellationWasDefault = box->objectName() == "sessionCleanupConfirmation" &&
                box->defaultButton() == box->button(QMessageBox::Cancel);
            box->button(QMessageBox::Cancel)->click();
        });
        panel.clearSavedSessions();
        QVERIFY(cancellationWasDefault);
        QVERIFY(QFile::exists(old));
        QVERIFY(QFile::exists(active));
        QTimer::singleShot(0, &panel, [&]
        {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) return;
            QTimer::singleShot(0, &panel, []
            {
                auto *summary = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
                if (summary) summary->accept();
            });
            box->button(QMessageBox::Yes)->click();
        });
        panel.clearSavedSessions();
        QVERIFY(!QFile::exists(old));
        QVERIFY(!QFile::exists(SessionEvents::fileName(old)));
        QVERIFY(QFile::exists(active));
        QVERIFY(QFile::exists(SessionEvents::fileName(active)));
        QVERIFY(panel.isRecording());
    }
    void cleanupProtectsPendingExportFromPreviousSession()
    {
        const auto directory = m_dir.filePath("cleanup-pending-export");
        QVERIFY(QDir().mkpath(directory));
        const auto previous = QSettings().value("recording/directory");
        QSettings().setValue("recording/directory", directory);
        auto restore = qScopeGuard([&] { QSettings().setValue("recording/directory", previous); });
        const auto old = createTestSession(directory, "old");
        WavePanel panel;
        panel.startRecording();
        panel.addData(0, {1, 2, 3, 4});
        const auto exporting = panel.bufferFileName();
        QSignalSpy completed(&panel, &WavePanel::exportFinished);
        QVERIFY(panel.exportDataTo(QDir(directory).filePath("keep.csv")));
        panel.clearData();
        const auto active = panel.bufferFileName();
        bool onlyOldWasListed = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) return;
            onlyOldWasListed = box->text().contains(QString::fromUtf8("1 个历史会话")) &&
                box->detailedText().contains("old.swave") &&
                !box->detailedText().contains(QFileInfo(exporting).fileName());
            box->button(QMessageBox::Cancel)->click();
        });
        panel.clearSavedSessions();
        QVERIFY(onlyOldWasListed);
        QVERIFY(QFile::exists(exporting));
        QVERIFY(QFile::exists(active));
        QVERIFY(QFile::exists(old));
        QTRY_COMPARE(completed.count(), 1);
        QVERIFY(completed[0][0].toBool());
    }
    void rawLookupCountsClosedIntervalsAndHandlesDuplicateTimes()
    {
        SampleArchive archive(sampleChannelCount);
        const auto path = m_dir.filePath("raw-lookup.swave");
        QVERIFY(archive.create(path));
        const double times[] = {0, 0, 1, 2, 4};
        for (int i = 0; i < 5; ++i)
        {
            double values[] = {double(i), 2, 3, 4, 50, 1};
            QVERIFY(archive.append(times[i], values));
        }
        QCOMPARE(archive.countInRange(0, 0), 2);
        QCOMPARE(archive.countInRange(1, 2), 2);
        QCOMPARE(archive.countInRange(.1, .9), 0);
        QCOMPARE(archive.countInRange(4, 4), 1);
        QCOMPARE(archive.countInRange(0, 10), 5);
        QCOMPARE(archive.countInRange(-1, 1), -1);
        QCOMPARE(archive.countInRange(2, 1), -1);
        QCOMPARE(archive.countInRange(0, qQNaN()), -1);
        double time = -1;
        QVector<double> values;
        QVERIFY(archive.nearestSample(0, time, values));
        QCOMPARE(time, 0.0);
        QCOMPARE(values[0], 0.0);
        QVERIFY(archive.nearestSample(.5, time, values));
        QCOMPARE(time, 0.0); // 等距时选择更早时间
        QVERIFY(archive.nearestSample(1.6, time, values));
        QCOMPARE(time, 2.0);
        QCOMPARE(values[0], 3.0);
        QVERIFY(archive.nearestSample(100, time, values));
        QCOMPARE(time, 4.0);
        QVERIFY(!archive.nearestSample(-1, time, values));
        QVERIFY(!archive.nearestSample(qQNaN(), time, values));
        SampleArchive reader;
        QVERIFY(reader.open(path));
        QCOMPARE(reader.countInRange(0, 4), 5);
    }
    void measurementUsesRawSamplesAndAllIntervalStatistics()
    {
        const auto path = m_dir.filePath("raw-measurement.swave");
        SampleArchive archive(sampleChannelCount);
        QVERIFY(archive.create(path));
        qint64 validActuator = 0;
        for (int i = 0; i < 10000; ++i)
        {
            const bool valid = i % 100 <= 50;
            double values[] = {i * .01, 2, 1500, 1500, valid ? double(i % 100) : qQNaN(),
                               valid ? 2.0 : qQNaN()};
            QVERIFY(archive.append(i * .001, values));
            if (i >= 1234 && i <= 7654 && valid) ++validActuator;
        }
        QVERIFY(archive.flush());
        const auto cancel = std::make_shared<std::atomic_bool>(false);
        const auto view = readSessionView(path, 0, 10, cancel);
        bool found = false;
        for (const auto &point : view.points)
            found |= qAbs(point.time - 1.234) < 1e-9;
        QVERIFY(!found); // 该采样不是绘图抽稀后的保留点
        const auto measurement = readSessionMeasurement(path, 1.23405, true, 7.65404, cancel);
        QVERIFY(measurement.error.isEmpty());
        QVERIFY(measurement.hasA && measurement.hasB);
        QVERIFY(qAbs(measurement.a.time - 1.234) < 1e-9);
        QVERIFY(qAbs(measurement.a.values[0] - 12.34) < 1e-9);
        QVERIFY(qAbs(measurement.b.time - 7.654) < 1e-9);
        QVERIFY(qIsNaN(measurement.b.values[4]));
        QCOMPARE(measurement.count, 6421);
        QCOMPARE(measurement.stats[0].count, 6421);
        QVERIFY(qAbs(measurement.stats[0].mean - 44.44) < 1e-8);
        QCOMPARE(measurement.stats[4].count, validActuator);
        const auto reverse = readSessionMeasurement(path, 7.65404, true, 1.23405, cancel);
        QCOMPARE(reverse.count, measurement.count);
        QCOMPARE(reverse.stats[0].mean, measurement.stats[0].mean);
        QVERIFY(reverse.b.time < reverse.a.time);
        const auto one = readSessionMeasurement(path, 1.23405, false, 0, cancel);
        QVERIFY(one.hasA && !one.hasB);
        const auto bad = readSessionMeasurement(path, qQNaN(), true, 0, cancel);
        QVERIFY(!bad.error.isEmpty());
    }
    void rangeExportPreservesTimesMetadataAndExistingDestinations()
    {
        const auto path = m_dir.filePath("range-source.swave");
        SampleArchive archive(sampleChannelCount);
        QVERIFY(archive.create(path));
        for (int i = 0; i < 10; ++i)
        {
            double values[] = {double(i), 2, 3, 4, double(i * 10), 2};
            QVERIFY(archive.append(i, values));
        }
        QVERIFY(archive.flush());
        auto state = std::make_shared<ExportState>();
        state->total = archive.countInRange(2, 5);
        state->selectedRange = true; state->lower = 2; state->upper = 5;
        const auto json = m_dir.filePath("selected-range.json");
        QVERIFY(exportSession(path, json, state).ok);
        QFile file(json);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(object["range_start_s"].toDouble(), 2.0);
        QCOMPARE(object["range_end_s"].toDouble(), 5.0);
        const auto samples = object["samples"].toArray();
        QCOMPARE(samples.size(), 4);
        QCOMPARE(samples[0].toObject()["time_s"].toDouble(), 2.0);
        QCOMPARE(samples[3].toObject()["time_s"].toDouble(), 5.0);
        QCOMPARE(samples[3].toObject()["actuator_position_pct"].toDouble(), 50.0);
        const auto csv = m_dir.filePath("selected-range.csv");
        QVERIFY(exportSession(path, csv, state).ok);
        QFile csvFile(csv);
        QVERIFY(csvFile.open(QIODevice::ReadOnly));
        const auto lines = csvFile.readAll().trimmed().split('\n');
        QCOMPARE(lines.size(), 5);
        QVERIFY(lines[1].startsWith("2,"));
        QVERIFY(lines[4].startsWith("5,"));
        csvFile.close();
        QVERIFY(csvFile.open(QIODevice::WriteOnly));
        csvFile.write("keep-existing"); csvFile.close();
        state->cancelled.store(true);
        QVERIFY(exportSession(path, csv, state).cancelled);
        QVERIFY(csvFile.open(QIODevice::ReadOnly));
        QCOMPARE(csvFile.readAll(), QByteArray("keep-existing"));
        csvFile.close();
        state->cancelled.store(false);
        state->lower = 5; state->upper = 2;
        QVERIFY(!exportSession(path, csv, state).ok);
        QVERIFY(csvFile.open(QIODevice::ReadOnly));
        QCOMPARE(csvFile.readAll(), QByteArray("keep-existing"));
        QVERIFY(!exportSession(path, path, state).ok);
    }
    void historyDownsamplingPreservesMissingActuatorFeedback()
    {
        const auto path = m_dir.filePath("missing-feedback-gap.swave");
        SampleArchive archive(sampleChannelCount);
        QVERIFY(archive.create(path));
        for (int i = 0; i < 10000; ++i)
        {
            const bool missing = i >= 1000 && i <= 8000;
            double values[] = {1, 2, 1500, 1500, missing ? qQNaN() : 50.0,
                               missing ? qQNaN() : 1.0};
            QVERIFY(archive.append(i * 1e-5, values));
        }
        QVERIFY(archive.flush());
        const auto view = readSessionView(path, 0, 1000, std::make_shared<std::atomic_bool>(false));
        bool hasGap = false;
        for (const auto &point : view.points)
            if (qIsNaN(point.values[4]) && qIsFinite(point.values[0])) hasGap = true;
        QVERIFY(hasGap);
        QVERIFY(view.points.size() < 100);
        QCOMPARE(view.count, 10000);
        QCOMPARE(view.stats[4].count, 2999);
    }
    void measurementControlsReadRawDataFreezeAndReset()
    {
        WavePanel panel;
        panel.resize(1200, 1000);
        panel.show();
        panel.startRecording();
        auto *mode = panel.findChild<QPushButton *>("measureModeButton");
        auto *exportButton = panel.findChild<QPushButton *>("exportSelectionButton");
        QVERIFY(!mode->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&panel, "togglePause", Q_ARG(bool, true)));
        panel.addData(0, {0, 2, 0, 1500, 50, 1});
        QVERIFY(mode->isEnabled()); // 空视图先冻结再收到数据，也能测量
        for (int i = 1; i <= 200; ++i)
            panel.addData(i * .1, {double(i), 2, double(i * 10), 1500, i % 2 ? qQNaN() : 50.0, 1});
        QVERIFY(panel.selectMeasurementPoint(5.12));
        QVERIFY(panel.selectMeasurementPoint(1.22)); // 上一轮查询未处理时继续选择 B
        QTRY_VERIFY(exportButton->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&panel, "onReplotTimeout"));
        for (auto *plot : panel.findChildren<QCustomPlot *>())
        {
            int marks = 0;
            for (int i = 0; i < plot->itemCount(); ++i)
                if (auto *label = dynamic_cast<QCPItemText *>(plot->item(i)))
                    if (label->text() == "A" || label->text() == "B")
                    {
                        QVERIFY(label->visible());
                        QVERIFY(qAbs(label->position->coords().x() -
                            (label->text() == "A" ? 5.1 : 1.2)) < 1e-9);
                        ++marks;
                    }
            QCOMPARE(marks, 2);
        }
        const auto info = panel.findChild<QLabel *>("measurementInfo")->text();
        QVERIFY(info.contains("5.100000"));
        QVERIFY(info.contains("1.200000"));
        QVERIFY(info.contains("-3.900000"));
        panel.showMeasurementDetails();
        auto *table = panel.findChild<QTableWidget *>("measurementTable");
        QVERIFY(table);
        QCOMPARE(table->item(0, 1)->text(), QString("51.000"));
        QCOMPARE(table->item(0, 2)->text(), QString("12.000"));
        QCOMPARE(table->item(0, 3)->text(), QString("-39.000"));
        QCOMPARE(table->item(0, 8)->text(), QString("40"));
        QCOMPARE(table->item(4, 1)->text(), QString("--"));
        QCOMPARE(table->item(4, 8)->text(), QString("20"));
        QVERIFY(!panel.selectMeasurementPoint(1000));
        QVERIFY(exportButton->isEnabled());
        panel.addData(20.1, {201, 2, 2010, 1500, 51, 2});
        QCOMPARE(panel.sampleCount(), 202); // 冻结测量不停止记录
        QSignalSpy exported(&panel, &WavePanel::exportFinished);
        const auto output = m_dir.filePath("gui-selected.csv");
        QVERIFY(panel.exportRangeTo(output, 2, 3));
        QTRY_COMPARE(exported.count(), 1);
        QVERIFY(exported[0][0].toBool());
        QFile file(output);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll().trimmed().split('\n').size(), 12);
        QVERIFY(!panel.exportRangeTo(m_dir.filePath("empty.csv"), 1000, 2000));
        QVERIFY(!QFile::exists(m_dir.filePath("empty.csv")));
        panel.clearMeasurement();
        QVERIFY(!exportButton->isEnabled());
        panel.showWholeSession();
        auto *quality = panel.findChild<QCustomPlot *>("qualityPlot");
        auto *actuator = panel.findChild<QCustomPlot *>("actuatorPlot");
        QTRY_VERIFY(quality->graph(0)->dataCount() > 0);
        QTest::qWait(50);
        QTest::mouseClick(quality, Qt::LeftButton, Qt::NoModifier,
            QPoint(qRound(quality->xAxis->coordToPixel(2.5)), quality->axisRect()->rect().center().y()));
        QTRY_VERIFY(panel.findChild<QPushButton *>("measurementDetailsButton")->isEnabled());
        QTest::mouseClick(actuator, Qt::LeftButton, Qt::NoModifier,
            QPoint(qRound(actuator->xAxis->coordToPixel(9.3)), actuator->axisRect()->rect().center().y()));
        QTRY_VERIFY(exportButton->isEnabled());
        const auto screenshot = qEnvironmentVariable("SERIAL_WAVE_SCREENSHOT");
        if (!screenshot.isEmpty())
        {
            panel.showMeasurementDetails();
            QVERIFY(table->window()->grab().save(screenshot + ".measure-details.png"));
            table->window()->hide();
            QVERIFY(QMetaObject::invokeMethod(&panel, "onReplotTimeout"));
            QTest::qWait(50);
            QVERIFY(panel.grab().save(screenshot + ".measure.png"));
        }
        QVERIFY(panel.selectMeasurementPoint(10));
        panel.clearData(); // 未完成读取不能把上一会话的测量结果写回新会话
        QVERIFY(!mode->isChecked());
        QTest::qWait(80);
        QVERIFY(!exportButton->isEnabled());
        QVERIFY(!panel.findChild<QLabel *>("measurementInfo")->isVisible());
        QVERIFY(quality->interactions().testFlag(QCP::iRangeDrag));
    }
    void wholeSessionAndTimeJumpKeepThreePlotsSynchronized()
    {
        WavePanel panel;
        panel.startRecording();
        for (int i = 0; i <= 200; ++i) panel.addData(i * .5, {1, 2, 3, 4, 50, 1});
        panel.showWholeSession();
        auto *quality = panel.findChild<QCustomPlot *>("qualityPlot");
        QCOMPARE(quality->xAxis->range().lower, 0.0);
        QCOMPARE(quality->xAxis->range().upper, 100.0);
        panel.jumpToTime(40);
        QCOMPARE(quality->xAxis->range().lower, 35.0);
        QCOMPARE(quality->xAxis->range().upper, 45.0);
        panel.jumpToTime(-1);
        panel.jumpToTime(1000);
        QCOMPARE(quality->xAxis->range().lower, 35.0);
        panel.findChild<QDoubleSpinBox *>("jumpTimeInput")->setValue(60);
        panel.findChild<QPushButton *>("jumpTimeButton")->click();
        for (auto *plot : panel.findChildren<QCustomPlot *>())
        {
            QCOMPARE(plot->xAxis->range().lower, 55.0);
            QCOMPARE(plot->xAxis->range().upper, 65.0);
        }
        QCOMPARE(panel.sampleCount(), 201);
        QVERIFY(panel.isRecording());
        panel.jumpToTime(0);
        QCOMPARE(quality->xAxis->range().lower, 0.0);
    }
    void frozenViewIncludesSamplesAppendedWhileReadWasFinishing()
    {
        WavePanel panel;
        panel.startRecording();
        panel.addData(0, {1, 2, 3, 4, 50, 1});
        panel.addData(.1, {1, 2, 3, 4, 50, 1});
        auto *plot = panel.findChild<QCustomPlot *>("qualityPlot");
        plot->xAxis->setRange(0, 5); // 排队一次冻结视图读取
        QVERIFY(QMetaObject::invokeMethod(&panel, "loadView"));
        QTest::qSleep(100); // 工作线程完成，暂不派发 GUI finished 槽
        panel.addData(.2, {9, 2, 3, 4, 60, 2});
        QTRY_VERIFY(plot->graph(0)->dataCount() == 3);
        QCOMPARE((plot->graph(0)->data()->constEnd() - 1)->key, .2);
        QCOMPARE((plot->graph(0)->data()->constEnd() - 1)->value, 9.0);
    }
    void cleanupProtectsCancelledMeasurementUntilItsResultIsHandled()
    {
        const auto directory = m_dir.filePath("cleanup-measurement");
        QVERIFY(QDir().mkpath(directory));
        const auto previous = QSettings().value("recording/directory");
        QSettings().setValue("recording/directory", directory);
        auto restore = qScopeGuard([&] { QSettings().setValue("recording/directory", previous); });
        const auto old = createTestSession(directory, "old");
        WavePanel panel;
        panel.startRecording();
        panel.addData(0, {1, 2, 3, 4, 50, 1});
        const auto measuring = panel.bufferFileName();
        QVERIFY(panel.selectMeasurementPoint(0));
        panel.clearData();
        bool protectedWhileFinishing = false;
        QTimer::singleShot(0, &panel, [&]
        {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box) return;
            protectedWhileFinishing = box->text().contains(QString::fromUtf8("1 个历史会话")) &&
                box->detailedText().contains("old.swave") &&
                !box->detailedText().contains(QFileInfo(measuring).fileName());
            box->button(QMessageBox::Cancel)->click();
        });
        panel.clearSavedSessions();
        QVERIFY(protectedWhileFinishing);
        QVERIFY(QFile::exists(measuring));
        QVERIFY(QFile::exists(old));
    }
    void comparisonPlotsAndRender()
    {
        MainWindow w;
        w.show();
        w.resize(1700, 1300);
        w.resetLayout();
        auto *sensor = w.findChild<CanPanel *>("sensorCanPanel");
        sensor->connectionChanged(true);
        auto *wave = w.findChild<WavePanel *>();
        for (int i = 0; i < 400; ++i)
            wave->addData(i * .05, {10 + 2 * std::sin(i * .1), 2 + .5 * std::cos(i * .08),
                                    1450 + 30 * std::sin(i * .07), 1500,
                                    50 + 40 * std::sin(i * .015), double(ActuatorState::Extending)});
        MachineStatus status;
        status.crushRate = 11.6;
        status.crushRateAvg = 10.1;
        status.impurityRate = 2.4;
        status.impurityRateAvg = 2;
        status.rpm = 1450;
        status.rpmTarget = 1500;
        status.actuatorPosition = 50 + 40 * std::sin(399 * .015);
        status.actuatorState = ActuatorState::Extending;
        status.running = true;
        status.frames = 400;
        status.updateHz = 20;
        w.findChild<StatusPanel *>()->updateStatus(status);
        QMetaObject::invokeMethod(w.findChild<RecordingPanel *>(), "refreshStats");
        QTest::qWait(150);
        const auto plots = wave->findChildren<QCustomPlot *>();
        QCOMPARE(plots.size(), 3);
        QVERIFY(plots[0]->graph(0)->dataCount() > 0);
        QCOMPARE(wave->findChild<QCustomPlot *>("speedPlot")->graph(1)->data()->constBegin()->value,
                 1500.);
        QCOMPARE(plots[0]->xAxis->range().upper, plots[1]->xAxis->range().upper);
        auto *speed = wave->findChild<QCustomPlot *>("speedPlot");
        auto *target = speed->graph(1);
        auto *legendItem = speed->legend->itemWithPlottable(target);
        QVERIFY(legendItem);
        speed->replot();
        QVERIFY(speed->rect().contains(legendItem->outerRect().center()));
        const auto recorded = wave->sampleCount();
        const bool targetVisible = target->visible();
        QTest::mouseClick(speed, Qt::LeftButton, Qt::NoModifier, legendItem->outerRect().center());
        QCOMPARE(target->visible(), !targetVisible);
        QTest::mouseClick(speed, Qt::LeftButton, Qt::NoModifier, legendItem->outerRect().center());
        QCOMPARE(target->visible(), targetVisible);
        QCOMPARE(wave->sampleCount(), recorded);
        const auto output = qEnvironmentVariable("SERIAL_WAVE_SCREENSHOT");
        if (!output.isEmpty())
            QVERIFY(w.grab().save(output));
        w.close();
    }
};
QTEST_MAIN(SerialWaveTests)
#include "test_serial_wave.moc"
