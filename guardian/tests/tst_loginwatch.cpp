// SPDX-License-Identifier: Apache-2.0
#include "LoginWatch.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QSignalSpy>
#include <QTest>

namespace {

constexpr auto login1 = QLatin1StringView("org.freedesktop.login1");
constexpr auto managerPath = QLatin1StringView("/org/freedesktop/login1");
constexpr auto manager = QLatin1StringView("org.freedesktop.login1.Manager");

// A second connection to the test's own session bus (CTest starts one with
// dbus-run-session), standing in for systemd-logind.
class StandInLogind {
public:
    explicit StandInLogind(const QString& name)
        : m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, name)) {}
    ~StandInLogind() {
        QDBusConnection::disconnectFromBus(m_bus.name());
    }

    bool takeLogindsName() {
        return m_bus.registerService(login1);
    }

    void send(const QString& signalName, const QString& id) {
        QDBusMessage signal = QDBusMessage::createSignal(managerPath, manager, signalName);
        signal << id << QVariant::fromValue(QDBusObjectPath(managerPath + "/session/" + id));
        m_bus.send(signal);
    }

private:
    QDBusConnection m_bus;
};

class LoginWatchTest : public QObject {
    Q_OBJECT

    // Who logind would say is logged in, as the test sets it.
    QSet<QString> m_now;

    LoginWatch::Read reader() {
        return [this] { return m_now; };
    }

private slots:
    void initTestCase() {
        QVERIFY2(QDBusConnection::sessionBus().isConnected(),
                 "run under dbus-run-session, as CTest does");
    }

    void init() {
        m_now.clear();
    }

    void readsWhoIsLoggedInAtOnce() {
        m_now = {"ada"};
        const LoginWatch watch(reader());
        QVERIFY(watch.isLoggedIn("ada"));
        QVERIFY(!watch.isLoggedIn("ben"));
    }

    // Read again by hand, it says changed only when someone came or went.
    void changedOnlyWhenSomeoneCameOrWent() {
        LoginWatch watch(reader());
        const QSignalSpy changed(&watch, &LoginWatch::changed);
        watch.refresh();
        QCOMPARE(changed.count(), 0);
        m_now = {"ben"};
        watch.refresh();
        QCOMPARE(changed.count(), 1);
        QVERIFY(watch.isLoggedIn("ben"));
        watch.refresh();
        QCOMPARE(changed.count(), 1);
    }

    void aSessionComingIsReadAgain() {
        StandInLogind logind(QStringLiteral("logind-new"));
        QVERIFY(logind.takeLogindsName());
        LoginWatch watch(reader());
        watch.listen(QDBusConnection::sessionBus());
        const QSignalSpy changed(&watch, &LoginWatch::changed);
        m_now = {"ada"};
        logind.send(QStringLiteral("SessionNew"), QStringLiteral("c3"));
        QTRY_COMPARE(changed.count(), 1);
        QVERIFY(watch.isLoggedIn("ada"));
    }

    void aSessionGoingIsReadAgain() {
        StandInLogind logind(QStringLiteral("logind-removed"));
        QVERIFY(logind.takeLogindsName());
        m_now = {"ada"};
        LoginWatch watch(reader());
        watch.listen(QDBusConnection::sessionBus());
        const QSignalSpy changed(&watch, &LoginWatch::changed);
        m_now.clear();
        logind.send(QStringLiteral("SessionRemoved"), QStringLiteral("c3"));
        QTRY_COMPARE(changed.count(), 1);
        QVERIFY(!watch.isLoggedIn("ada"));
    }

    // Any program can send the signal, but only logind's is listened to.
    void anotherSenderIsIgnored() {
        StandInLogind logind(QStringLiteral("logind-owner"));
        QVERIFY(logind.takeLogindsName());
        StandInLogind other(QStringLiteral("someone-else"));
        LoginWatch watch(reader());
        watch.listen(QDBusConnection::sessionBus());
        const QSignalSpy changed(&watch, &LoginWatch::changed);
        m_now = {"ada"};
        other.send(QStringLiteral("SessionNew"), QStringLiteral("c3"));
        QTest::qWait(200);
        QCOMPARE(changed.count(), 0);
    }
};

} // namespace

QTEST_GUILESS_MAIN(LoginWatchTest)
#include "tst_loginwatch.moc"
