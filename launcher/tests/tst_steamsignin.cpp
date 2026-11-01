// SPDX-License-Identifier: Apache-2.0
#include "SteamSignIn.h"

#include <QDateTime>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Now = SteamSignIn::Now;

// A line as Steam writes it at `time` on 2026-10-27.
QByteArray line(const QByteArray& time, const QByteArray& says) {
    return "[2026-10-27 " + time + "] " + says + '\n';
}

// A client starting, as it begins its part of the log.
QByteArray clientStarts(const QByteArray& time) {
    return line(time, "Client version: 1792618285") + line(time, "Console Log Start");
}

// The line a client writes as it signs in.
QByteArray signedIn(const QByteArray& time) {
    return line(time, "Waiting for compat in post-logon took: 0.096359s");
}

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// When the test reads the logs: the morning after their lines. It matters
// only for a time in an hour the clocks change in, which tst_steamlogtime
// tests.
QDateTime nextMorning() {
    return {QDate(2026, 10, 28), QTime(9, 0)};
}

// The two logs of one Steam in a folder of the test's own.
struct Logs {
    QTemporaryDir dir;
    QString console = dir.filePath(QStringLiteral("console_log.txt"));
    QString bootstrap = dir.filePath(QStringLiteral("bootstrap_log.txt"));
    Now now() const {
        return SteamSignIn(console, bootstrap).read(nextMorning());
    }
};

// Whether Steam could sign in (#182). The lines are ones Steam wrote on the
// kids' laptop on 2026-10-27 and on the development laptop on 2026-10-28.
class SteamSignInTest : public QObject {
    Q_OBJECT

private slots:
    void aMissingLogSaysNothingIsWrong() {
        const Logs logs;
        QCOMPARE(logs.now(), Now::Fine);
    }

    void aClientThatSignedInIsFine() {
        const Logs logs;
        append(logs.console, clientStarts("19:16:45") + signedIn("19:16:51"));
        QCOMPARE(logs.now(), Now::Fine);
    }

    // The evening of #182: the saved sign-in was turned down, and every try
    // after it failed with No Connection. The refusal still counts.
    void aRefusalCountsThroughTheNoConnectionsAfterIt() {
        const Logs logs;
        append(logs.console,
               clientStarts("23:11:40") + line("23:12:03", "LogonFailure Access Denied"));
        QCOMPARE(logs.now(), Now::Refused);
        append(logs.console, line("23:13:07", "LogonFailure No Connection") +
                                 line("23:14:19", "LogonFailure No Connection"));
        QCOMPARE(logs.now(), Now::Refused);
    }

    // On 2026-10-28 a grown-up signed in again two minutes after the refusal.
    void signingInAgainClearsARefusal() {
        const Logs logs;
        append(logs.console,
               clientStarts("13:12:58") + line("13:13:05", "LogonFailure Access Denied") +
                   line("13:14:13", "LogonFailure No Connection") +
                   line("13:14:56", "GetWin32Stats: display was not open yet, good") +
                   signedIn("13:14:59") + line("13:14:59", "System startup time: 120.89 seconds"));
        QCOMPARE(logs.now(), Now::Fine);
    }

    // The Wi-Fi going down while Steam ran, then coming back.
    void noConnectionUntilSteamSignsInAgain() {
        const Logs logs;
        append(logs.console, clientStarts("15:38:00") +
                                 line("15:38:25", "LogonFailure No Connection") +
                                 line("15:38:44", "LogonFailure No Connection"));
        QCOMPARE(logs.now(), Now::NoConnection);
        append(logs.console, signedIn("15:43:16"));
        QCOMPARE(logs.now(), Now::Fine);
    }

    // A refusal from an earlier client says nothing about the one that runs
    // now, which tries the sign-in again.
    void aNewClientStartsAfresh() {
        const Logs logs;
        append(logs.console,
               clientStarts("13:12:58") + line("13:13:05", "LogonFailure Access Denied"));
        append(logs.console, clientStarts("19:16:45"));
        QCOMPARE(logs.now(), Now::Fine);
    }

    // While the updater installs a new client, the console log still ends
    // with the last client's lines. They are from before this start.
    void linesFromBeforeTheUpdaterStartedDoNotCount() {
        const Logs logs;
        append(logs.console,
               clientStarts("13:12:58") + line("13:13:05", "LogonFailure No Connection"));
        append(logs.bootstrap, line("19:16:40", "Startup - updater built Oct 13 2026 18:44:25") +
                                   line("19:16:40", "Found pending update"));
        QCOMPARE(logs.now(), Now::Fine);
        append(logs.console,
               clientStarts("19:17:30") + line("19:17:40", "LogonFailure Access Denied"));
        QCOMPARE(logs.now(), Now::Refused);
    }

    // `LogonFailureReceived` is in Steam's connection log, not a failure line.
    void onlyALogonFailureLineCounts() {
        const Logs logs;
        append(logs.console, clientStarts("13:12:58") +
                                 line("13:13:05", "Clearing in-memory token - 15 (Access Denied): "
                                                  "LogonFailureReceived(2)"));
        QCOMPARE(logs.now(), Now::Fine);
    }

    // Which reasons Steam tries again by itself. The ones it does not try
    // again come from Steam's EResult names, as the client writes them.
    void whichReasonsSteamTriesAgain_data() {
        QTest::addColumn<QByteArray>("reason");
        QTest::addColumn<bool>("again");
        QTest::newRow("no connection") << QByteArray("No Connection") << true;
        QTest::newRow("service unavailable") << QByteArray("Service Unavailable") << true;
        QTest::newRow("try another server") << QByteArray("Try another CM") << true;
        QTest::newRow("timeout") << QByteArray("Timeout") << true;
        QTest::newRow("failure") << QByteArray("Failure") << true;
        QTest::newRow("access denied") << QByteArray("Access Denied") << false;
        QTest::newRow("invalid password") << QByteArray("Invalid Password") << false;
        QTest::newRow("account logon denied") << QByteArray("Account Logon Denied") << false;
        QTest::newRow("auth code") << QByteArray("Invalid Login Auth Code") << false;
        QTest::newRow("rate limit") << QByteArray("Rate Limit Exceeded") << false;
    }

    void whichReasonsSteamTriesAgain() {
        QFETCH(const QByteArray, reason);
        QFETCH(const bool, again);
        QCOMPARE(SteamSignIn::triesAgain(reason), again);
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamSignInTest)
#include "tst_steamsignin.moc"
