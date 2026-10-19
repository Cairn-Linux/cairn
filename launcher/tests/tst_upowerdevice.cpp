// SPDX-License-Identifier: Apache-2.0
#include "UPowerDevice.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QSignalSpy>
#include <QTest>

namespace {

constexpr auto upower = QLatin1StringView("org.freedesktop.UPower");
constexpr auto path = QLatin1StringView("/org/freedesktop/UPower/devices/DisplayDevice");
constexpr auto deviceInterface = QLatin1StringView("org.freedesktop.UPower.Device");
constexpr auto noReply = "Could not read the battery from UPower "
                         "(org.freedesktop.DBus.Error.ServiceUnknown), so the launcher shows no "
                         "battery.";

// The part of UPower's DisplayDevice the launcher reads, exported the way
// UPower exports it, so QtDBus answers Get and GetAll for it. The values are
// a laptop at 42 percent, discharging, not low.
class DisplayDevice : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.UPower.Device")
    Q_PROPERTY(bool IsPresent MEMBER isPresent)
    Q_PROPERTY(double Percentage MEMBER percentage)
    Q_PROPERTY(uint State MEMBER state)
    Q_PROPERTY(uint WarningLevel MEMBER warningLevel)
    Q_PROPERTY(uint Type MEMBER type)

public:
    bool isPresent = true;
    double percentage = 42;
    uint state = 2;
    uint warningLevel = 1;
    uint type = 2;
};

// A second connection to the test's own session bus (CTest starts one with
// dbus-run-session), standing in for UPower.
class StandInUPower {
public:
    explicit StandInUPower(const QString& name)
        : m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, name)) {
        m_bus.registerObject(path, &device, QDBusConnection::ExportAllProperties);
    }
    // Gives the name back before the connection goes, so the next test's
    // stand-in can take it at once.
    ~StandInUPower() {
        m_bus.unregisterService(upower);
        QDBusConnection::disconnectFromBus(m_bus.name());
    }

    bool takeName() {
        return m_bus.registerService(upower);
    }

    bool dropName() {
        return m_bus.unregisterService(upower);
    }

    // UPower sends PropertiesChanged by hand; so does the stand-in.
    bool announce(const QVariantMap& changed, const QStringList& invalidated = {},
                  const QString& interface = deviceInterface) {
        QDBusMessage signal =
            QDBusMessage::createSignal(path, QStringLiteral("org.freedesktop.DBus.Properties"),
                                       QStringLiteral("PropertiesChanged"));
        signal << interface << changed << invalidated;
        return m_bus.send(signal);
    }

    DisplayDevice device;

private:
    QDBusConnection m_bus;
};

// Every question the reader asked has had its answer.
bool answered(const UPowerDevice& reader) {
    return reader.findChildren<QDBusPendingCallWatcher*>().isEmpty();
}

class UPowerDeviceTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY2(QDBusConnection::sessionBus().isConnected(),
                 "run under dbus-run-session, as CTest does");
    }

    // First, before any stand-in has had UPower's name.
    void noUPowerIsNoBattery() {
        QTest::ignoreMessage(QtWarningMsg, noReply);
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        reader.setService(upower);
        QTRY_VERIFY(answered(reader));
        QCOMPARE(reader.isPresent(), false);
    }

    void readsTheDisplayDevice() {
        StandInUPower stand(QStringLiteral("reads"));
        QVERIFY(stand.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        QCOMPARE(reader.isPresent(), false);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        QCOMPARE(reader.percentage(), 42.0);
        QCOMPARE(reader.state(), 2);
        QCOMPARE(reader.warningLevel(), 1);
        QCOMPARE(reader.type(), 2);
    }

    // A change names only what changed, and the rest stays as it was.
    void followsPropertiesChanged() {
        StandInUPower stand(QStringLiteral("follows"));
        QVERIFY(stand.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        QVERIFY(stand.announce(
            {{QStringLiteral("Percentage"), 12.0}, {QStringLiteral("WarningLevel"), 3U}}));
        QTRY_COMPARE(reader.warningLevel(), 3);
        QCOMPARE(reader.percentage(), 12.0);
        QCOMPARE(reader.state(), 2);
        // Another interface's change is not the device's.
        QVERIFY(stand.announce({{QStringLiteral("Percentage"), 99.0}}, {},
                               QStringLiteral("org.freedesktop.UPower")));
        QVERIFY(stand.announce({{QStringLiteral("State"), 1U}}));
        QTRY_COMPARE(reader.state(), 1);
        QCOMPARE(reader.percentage(), 12.0);
    }

    void nothingNewIsNoChange() {
        StandInUPower stand(QStringLiteral("nothing-new"));
        QVERIFY(stand.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        const QSignalSpy changed(&reader, &UPowerDevice::readingChanged);
        QVERIFY(stand.announce({{QStringLiteral("Percentage"), 42.0}}));
        QVERIFY(stand.announce({{QStringLiteral("State"), 1U}}));
        QTRY_COMPARE(reader.state(), 1);
        QCOMPARE(changed.count(), 1);
    }

    void anInvalidatedPropertyIsReadAgain() {
        StandInUPower stand(QStringLiteral("invalidated"));
        QVERIFY(stand.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        stand.device.percentage = 77;
        QVERIFY(stand.announce({}, {QStringLiteral("Percentage")}));
        QTRY_COMPARE(reader.percentage(), 77.0);
    }

    // Any program can send PropertiesChanged on the bus, but only the one
    // that owns UPower's name is listened to.
    void anotherSenderIsIgnored() {
        StandInUPower stand(QStringLiteral("owner"));
        QVERIFY(stand.takeName());
        StandInUPower other(QStringLiteral("someone-else"));
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        QVERIFY(other.announce(
            {{QStringLiteral("Percentage"), 3.0}, {QStringLiteral("WarningLevel"), 4U}}));
        QTest::qWait(200);
        QCOMPARE(reader.percentage(), 42.0);
        QCOMPARE(reader.warningLevel(), 1);
        QVERIFY(stand.announce({{QStringLiteral("Percentage"), 41.0}}));
        QTRY_COMPARE(reader.percentage(), 41.0);
    }

    void upowerGoingAwayIsNoBattery() {
        StandInUPower stand(QStringLiteral("going"));
        QVERIFY(stand.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        QVERIFY(stand.dropName());
        QTRY_COMPARE(reader.isPresent(), false);
        QCOMPARE(reader.percentage(), 0.0);
        QCOMPARE(reader.type(), 0);
    }

    // A restarted UPower is a new program under the same name: it is read
    // again and followed, and the old one is not.
    void aRestartedUPowerIsReadAgain() {
        StandInUPower before(QStringLiteral("before"));
        QVERIFY(before.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        QVERIFY(before.dropName());
        QTRY_COMPARE(reader.isPresent(), false);
        StandInUPower after(QStringLiteral("after"));
        after.device.percentage = 55;
        QVERIFY(after.takeName());
        QTRY_COMPARE(reader.isPresent(), true);
        QCOMPARE(reader.percentage(), 55.0);
        QVERIFY(before.announce({{QStringLiteral("Percentage"), 3.0}}));
        QVERIFY(after.announce({{QStringLiteral("State"), 1U}}));
        QTRY_COMPARE(reader.state(), 1);
        QCOMPARE(reader.percentage(), 55.0);
    }

    void withoutABusItSaysSo() {
        QTest::ignoreMessage(QtWarningMsg, "No system bus, so the launcher shows no battery.");
        UPowerDevice reader(QDBusConnection(QStringLiteral("no-bus")), path);
        reader.setService(upower);
        QCOMPARE(reader.isPresent(), false);
        QVERIFY(answered(reader));
    }

    void aPathItCannotFollowSaysSo() {
        QTest::ignoreMessage(QtWarningMsg,
                             "Could not follow the battery in UPower, so the launcher shows no "
                             "battery.");
        StandInUPower stand(QStringLiteral("bad-path"));
        QVERIFY(stand.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), QStringLiteral("not a path"));
        reader.setService(upower);
        QTest::qWait(200);
        QCOMPARE(reader.isPresent(), false);
        QVERIFY(answered(reader));
    }

    // The default: a window made in a test asks no one.
    void anEmptyServiceAsksNothing() {
        StandInUPower stand(QStringLiteral("empty"));
        QVERIFY(stand.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        const QSignalSpy changed(&reader, &UPowerDevice::readingChanged);
        reader.setService(QString());
        QVERIFY(stand.announce({{QStringLiteral("Percentage"), 3.0}}));
        QTest::qWait(200);
        QCOMPARE(changed.count(), 0);
        QCOMPARE(reader.isPresent(), false);
        QVERIFY(answered(reader));
    }

    void aNewServiceForgetsTheOld() {
        StandInUPower stand(QStringLiteral("old"));
        QVERIFY(stand.takeName());
        UPowerDevice reader(QDBusConnection::sessionBus(), path);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        QTest::ignoreMessage(QtWarningMsg, noReply);
        reader.setService(QStringLiteral("org.example.NotUPower"));
        QCOMPARE(reader.isPresent(), false);
        QTRY_VERIFY(answered(reader));
        QVERIFY(stand.announce({{QStringLiteral("Percentage"), 3.0}}));
        QTest::qWait(200);
        QCOMPARE(reader.isPresent(), false);
        QCOMPARE(reader.percentage(), 0.0);
        reader.setService(upower);
        QTRY_COMPARE(reader.isPresent(), true);
        QCOMPARE(reader.percentage(), 42.0);
    }
};

} // namespace

QTEST_GUILESS_MAIN(UPowerDeviceTest)
#include "tst_upowerdevice.moc"
