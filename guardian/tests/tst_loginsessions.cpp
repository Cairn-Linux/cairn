// SPDX-License-Identifier: Apache-2.0
#include "LoginSessions.h"

#include <QTest>

namespace {

class LoginSessionsTest : public QObject {
    Q_OBJECT

private slots:
    void counts_data() {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("service");
        QTest::addColumn<bool>("login");
        QTest::newRow("a desktop login") << "user" << "sddm" << true;
        QTest::newRow("a text login") << "user" << "login" << true;
        QTest::newRow("an early login") << "user-early" << "sddm" << true;
        QTest::newRow("a greeter") << "greeter" << "sddm-greeter" << true;
        QTest::newRow("a background session") << "background" << "cron" << true;
        QTest::newRow("no service") << "user" << "" << true;
        QTest::newRow("the service manager") << "manager" << "systemd-user" << false;
        QTest::newRow("an early service manager") << "manager-early" << "systemd-user" << false;
        QTest::newRow("a Steam visit") << "user" << "cairn-steam-visit" << false;
        QTest::newRow("a session gone since it was listed") << "" << "sddm" << false;
    }

    // The visit helper's rule: every session but the service manager's and
    // a Steam visit's.
    void counts() {
        QFETCH(const QString, kind);
        QFETCH(const QString, service);
        QFETCH(const bool, login);
        QCOMPARE(LoginSessions::isLogin({.login = "ada", .kind = kind, .service = service}), login);
    }

    // Each account with a login of its own, once.
    void whoIsLoggedIn() {
        const QList<LoginSessions::Session> sessions{
            {.login = "ada", .kind = "manager", .service = "systemd-user"},
            {.login = "ada", .kind = "user", .service = "sddm"},
            {.login = "ada", .kind = "user", .service = "login"},
            {.login = "ben", .kind = "manager", .service = "systemd-user"},
            {.login = "ben", .kind = "user", .service = "cairn-steam-visit"},
            {.login = "dee", .kind = "user", .service = "sddm"},
            {.login = "guardian", .kind = "user", .service = "sddm"},
        };
        QCOMPARE(LoginSessions::loggedIn(sessions), QSet<QString>({"ada", "dee", "guardian"}));
        QVERIFY(LoginSessions::loggedIn({}).isEmpty());
    }

    // When the test machine has logind, it answers, and every session it
    // lists names an account. Nothing is assumed about who is logged in.
    void asksLogind() {
        const std::optional<QList<LoginSessions::Session>> sessions = LoginSessions::fromLogind();
        if (!sessions) {
            QSKIP("This machine's logind cannot be asked.");
        }
        for (const LoginSessions::Session& session : *sessions) {
            QVERIFY(!session.login.isEmpty());
        }
    }
};

} // namespace

QTEST_GUILESS_MAIN(LoginSessionsTest)
#include "tst_loginsessions.moc"
