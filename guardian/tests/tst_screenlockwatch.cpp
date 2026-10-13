// SPDX-License-Identifier: Apache-2.0
#include "ScreenLockWatch.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QTest>

namespace {

constexpr auto screenSaver = QLatin1StringView("org.freedesktop.ScreenSaver");
constexpr auto path = QLatin1StringView("/org/freedesktop/ScreenSaver");

// A second connection to the test's own session bus (CTest starts one with
// dbus-run-session), standing in for KWin.
class StandInKWin {
public:
    explicit StandInKWin(const QString& name)
        : m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, name)) {}
    ~StandInKWin() {
        QDBusConnection::disconnectFromBus(m_bus.name());
    }

    bool takeScreenSaverName() {
        return m_bus.registerService(screenSaver);
    }

    void sendActiveChanged(bool active, const QString& onPath = path) {
        QDBusMessage signal =
            QDBusMessage::createSignal(onPath, screenSaver, QStringLiteral("ActiveChanged"));
        signal << active;
        m_bus.send(signal);
    }

private:
    QDBusConnection m_bus;
};

class ScreenLockWatchTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY2(QDBusConnection::sessionBus().isConnected(),
                 "run under dbus-run-session, as CTest does");
    }

    void theLockScreenComingUpIsALock() {
        StandInKWin kwin(QStringLiteral("kwin-lock"));
        QVERIFY(kwin.takeScreenSaverName());
        const ScreenLockWatch watch(QDBusConnection::sessionBus());
        const QSignalSpy locked(&watch, &ScreenLockWatch::locked);
        kwin.sendActiveChanged(true);
        QTRY_COMPARE(locked.count(), 1);
    }

    void theLockScreenGoingIsNot() {
        StandInKWin kwin(QStringLiteral("kwin-unlock"));
        QVERIFY(kwin.takeScreenSaverName());
        const ScreenLockWatch watch(QDBusConnection::sessionBus());
        const QSignalSpy locked(&watch, &ScreenLockWatch::locked);
        kwin.sendActiveChanged(false);
        kwin.sendActiveChanged(true);
        QTRY_COMPARE(locked.count(), 1);
        QTest::qWait(100);
        QCOMPARE(locked.count(), 1);
    }

    // Any program of the Guardian's can send the signal, but only the one
    // that owns the screen saver's name is listened to.
    void anotherSenderIsIgnored() {
        StandInKWin kwin(QStringLiteral("kwin-owner"));
        QVERIFY(kwin.takeScreenSaverName());
        StandInKWin other(QStringLiteral("someone-else"));
        const ScreenLockWatch watch(QDBusConnection::sessionBus());
        const QSignalSpy locked(&watch, &ScreenLockWatch::locked);
        other.sendActiveChanged(true);
        QTest::qWait(200);
        QCOMPARE(locked.count(), 0);
        kwin.sendActiveChanged(true);
        QTRY_COMPARE(locked.count(), 1);
    }

    // KWin sends it on two paths at once; one lock is one lock.
    void theOtherPathIsNotASecondLock() {
        StandInKWin kwin(QStringLiteral("kwin-paths"));
        QVERIFY(kwin.takeScreenSaverName());
        const ScreenLockWatch watch(QDBusConnection::sessionBus());
        const QSignalSpy locked(&watch, &ScreenLockWatch::locked);
        kwin.sendActiveChanged(true, QStringLiteral("/ScreenSaver"));
        kwin.sendActiveChanged(true);
        QTRY_COMPARE(locked.count(), 1);
        QTest::qWait(100);
        QCOMPARE(locked.count(), 1);
    }

    void withoutABusItSaysSo() {
        QTest::ignoreMessage(QtWarningMsg,
                             "No session bus: locking the screen will not close a child's Steam.");
        const QDBusConnection none(QStringLiteral("no-bus"));
        const ScreenLockWatch watch(none);
    }
};

} // namespace

QTEST_GUILESS_MAIN(ScreenLockWatchTest)
#include "tst_screenlockwatch.moc"
