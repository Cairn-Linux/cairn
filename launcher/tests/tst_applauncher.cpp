// SPDX-License-Identifier: Apache-2.0
#include "AppLauncher.h"
#include "SteamAtLogin.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Every case uses /bin/sh so the test needs nothing beyond a POSIX shell.
QString shell() {
    return QStringLiteral("/bin/sh");
}

// The child's home data folder for these tests, so Steam's log is read from
// a folder of the test's, never from the Steam of whoever runs them.
const QTemporaryDir& dataHome() {
    static const QTemporaryDir home;
    return home;
}

// A launch step as Steam writes it in the child's own Steam log.
void steamSays(const QByteArray& appId, const QByteArray& says) {
    QFile log(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt")));
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write("[2026-10-20 15:30:43] GameAction [AppID " + appId + ", ActionID 1] : LaunchApp " +
              says + '\n');
}

// A line Steam writes in one of its logs, `secondsAgo` before now.
void steamLogs(const QString& name, qint64 secondsAgo, const QByteArray& says) {
    QFile log(dataHome().filePath(QStringLiteral("Steam/logs/") + name));
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    const QString time = QDateTime::currentDateTime()
                             .addSecs(-secondsAgo)
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    log.write('[' + time.toLatin1() + "] " + says + '\n');
}

// The client's own start command, which it runs once it has started.
QByteArray steamStartsItself() {
    return "ExecCommandLine: \"'/var/home/child/.local/share/Steam/ubuntu12_32/steam' "
           "'-srt-logger-opened' '-silent'\"";
}

// Steam taking a tap on Pajama Sam 2.
QByteArray pajamaSam2Taken() {
    return "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-applaunch' '292780'\"";
}

QStringList pajamaSam2() {
    return {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("292780")};
}

// The test VM's stuck run (#206): Steam took the tap, fetched scout and took
// the game off its schedule `secondsAgo`, and the launch stayed at
// DownloadingDepots.
void steamStopsPartWay(qint64 secondsAgo) {
    const QString content = QStringLiteral("content_log.txt");
    steamLogs(QStringLiteral("console_log.txt"), secondsAgo + 22, pajamaSam2Taken());
    steamSays("292780", "changed task to DownloadingDepots with \"\"");
    steamLogs(content, secondsAgo + 20,
              "AppID 1070560 scheduler update : Priority First, not played for 1 seconds, update "
              "disabled for 0 seconds");
    steamLogs(content, secondsAgo + 20,
              "AppID 1070560 update started : download 0/75258128, store 0/0, reuse 0/0, delta "
              "0/0, stage 0/223012461 ");
    steamLogs(content, secondsAgo,
              "AppID 1070560 finished update, 1 mounted depots (BuildID 26115262) : 1070561 "
              "(8475493586714137016),");
    steamLogs(content, secondsAgo,
              "AppID 1070560 scheduler finished : removed from schedule (result No Error, state "
              "0xc) ");
    steamLogs(content, secondsAgo,
              "AppID 292780 scheduler finished : removed from schedule (result Suspended, state "
              "0xc) ");
}

// The restarted Steam fetching soldier and taking the game off its schedule
// in the same second, as on the test VM's working run (#206).
void steamFetchesSoldier() {
    const QString content = QStringLiteral("content_log.txt");
    steamLogs(content, 0,
              "AppID 1391110 scheduler update : Priority First, not played for 1 seconds, update "
              "disabled for 0 seconds");
    steamLogs(content, 0,
              "AppID 1391110 update started : download 0/192183760, store 0/0, reuse 0/0, delta "
              "0/0, stage 0/677401998 ");
    steamLogs(content, 0, "AppID 1391110 finished update, 1 mounted depots : 1391111,");
    steamLogs(content, 0,
              "AppID 1391110 scheduler finished : removed from schedule (result No Error, state "
              "0xc) ");
    steamLogs(content, 0,
              "AppID 292780 scheduler finished : removed from schedule (result Suspended, state "
              "0xc) ");
}

// Steam's logs as no client has written them yet, so each case's lines are
// its own.
void forgetSteamsLogs() {
    for (const QString& name :
         {QStringLiteral("console_log.txt"), QStringLiteral("content_log.txt"),
          QStringLiteral("bootstrap_log.txt")}) {
        QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/") + name));
    }
}

// Stand-ins in `dir` for steam, systemctl and systemd-run, so starting Steam
// again (#206) runs against them. steam writes down how it was run, and
// -shutdown ends the service; systemctl prints the service's state from the
// file `unit` and succeeds only when it is active; systemd-run writes down
// what it was asked and makes the service active again.
void installStandIns(const QTemporaryDir& dir, const QByteArray& unitState) {
    const QByteArray folder = dir.path().toUtf8();
    const auto standIn = [&dir](const QString& name, const QByteArray& body) {
        QFile file(dir.filePath(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("#!/bin/sh\n" + body);
        file.close();
        QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    };
    standIn(QStringLiteral("steam"), "echo \"$@\" >> '" + folder +
                                         "/runs'\n"
                                         "if [ \"$1\" = -shutdown ]; then\n"
                                         "    echo inactive > '" +
                                         folder +
                                         "/unit'\n"
                                         "fi\n"
                                         "exit 0\n");
    standIn(QStringLiteral("systemctl"), "state=$(cat '" + folder +
                                             "/unit')\n"
                                             "echo \"$state\"\n"
                                             "[ \"$state\" = active ]\n");
    standIn(QStringLiteral("systemd-run"), R"(printf '%s\n' "$@" >> ')" + folder +
                                               "/started'\necho active > '" + folder + "/unit'\n");
    QFile unit(dir.filePath(QStringLiteral("unit")));
    QVERIFY(unit.open(QIODevice::WriteOnly));
    unit.write(unitState + '\n');
}

// The launcher's own SteamRestart, a child of it, whose finished signal says
// Steam was stopped and started again.
const SteamRestart* restartOf(const AppLauncher& launcher) {
    return launcher.findChild<SteamRestart*>();
}

// The lines of a file a stand-in wrote, or none.
QStringList linesOf(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

// Cases about whose a window is set the steady wait to zero, so the app is
// open the moment its window is up; the cases about the wait set it short.
class AppLauncherTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(dataHome().isValid());
        qputenv("XDG_DATA_HOME", dataHome().path().toUtf8());
        QVERIFY(QDir(dataHome().path()).mkpath(QStringLiteral("Steam/logs")));
    }

    void startsIdle() {
        const AppLauncher launcher(this);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(launcher.title().isEmpty());
    }

    // A tile with nothing set up yet says so and needs no grown-up.
    void emptyExecIsComingSoonAtOnce() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.launch(QStringLiteral("Music"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::ComingSoon);
        QVERIFY(!launcher.needsGrownUp());
        QCOMPARE(launcher.title(), QStringLiteral("Music"));
        QCOMPARE(states.count(), 1);
    }

    void missingProgramFails() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Draw"), {QStringLiteral("/nonexistent/cairn-app")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
    }

    // In the kiosk every program starts in a scope of its own, which is what
    // cairn-give-up ends (ADR-0025, #98). A stand-in systemd-run on PATH
    // writes down what it was asked and runs the program after the --.
    void aScopedLaunchStartsTheProgramInsideSystemdRun() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString log = dir.filePath(QStringLiteral("asked"));
        QFile fake(dir.filePath(QStringLiteral("systemd-run")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write(QStringLiteral("#!/bin/sh\n"
                                  "printf '%s\\n' \"$@\" > '%1'\n"
                                  "while [ \"$1\" != -- ]; do shift; done\n"
                                  "shift\n"
                                  "exec \"$@\"\n")
                       .arg(log)
                       .toUtf8());
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setProperty("scoped", true);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        // The program was found when it started, so PATH can go back now.
        qputenv("PATH", path);

        QTRY_VERIFY(QFileInfo(log).size() > 0);
        QFile asked(log);
        QVERIFY(asked.open(QIODevice::ReadOnly));
        const QString unit =
            QStringLiteral("--unit=cairn-app-%1-1").arg(QCoreApplication::applicationPid());
        QCOMPARE(QString::fromUtf8(asked.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts),
                 (QStringList{QStringLiteral("--user"), QStringLiteral("--scope"),
                              QStringLiteral("--quiet"), QStringLiteral("--collect"), unit,
                              QStringLiteral("--"), shell(), QStringLiteral("-c"),
                              QStringLiteral("exit 0")}));
        QVERIFY(launcher.state() != AppLauncher::State::Failed);
    }

    void earlyBadExitFails() {
        AppLauncher launcher(this);
        launcher.setSettleMilliseconds(2000);
        launcher.launch(QStringLiteral("Music"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 3")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.title(), QStringLiteral("Music"));
    }

    // No window ever opens, so the grace timer, not the clean exit, ends the
    // wait. Back at the tiles a child would press again; the grown-up screen
    // says the tile did not start (ADR-0030, #139).
    void launchThatOpensNoWindowNeedsAGrownUpAfterTheGrace() {
        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(150);
        launcher.launch(QStringLiteral("Story"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Failed, 5000);
        QVERIFY(launcher.needsGrownUp());
        QCOMPARE(launcher.title(), QStringLiteral("Story"));
    }

    // A program that ran past the settle window and quit without ever putting
    // a window up did not start either, whatever its exit code.
    void lateExitWithNoWindowNeedsAGrownUp() {
        AppLauncher launcher(this);
        launcher.setSettleMilliseconds(100);
        launcher.setLaunchGraceMilliseconds(60000);
        launcher.launch(QStringLiteral("Build"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2; exit 3")});
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Failed, 5000);
        QVERIFY(launcher.needsGrownUp());
    }

    // A game that comes after the grown-up screen is the game: it takes over,
    // and is not a window that opened on its own.
    void aWindowAfterNothingCameIsTheApp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setLaunchGraceMilliseconds(100);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QVERIFY(!launcher.needsGrownUp());
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // Back ends the launch: a window after that opened on its own, and the
    // next tile starts as usual.
    void backAfterNothingCameEndsTheLaunch() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setLaunchGraceMilliseconds(100);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Practice"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QCOMPARE(launcher.title(), QStringLiteral("Practice"));
    }

    // A program still running when nothing came runs on by itself; its exit
    // later neither ends the grown-up screen nor says anything new.
    void aLaterExitLeavesTheGrownUpScreen() {
        AppLauncher launcher(this);
        launcher.setSettleMilliseconds(50);
        launcher.setLaunchGraceMilliseconds(100);
        launcher.launch(QStringLiteral("Music"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.3; exit 3")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        // Saying it again would mean the exit was still being watched.
        QTest::failOnWarning(QRegularExpression(QStringLiteral("Nothing of Music")));
        QTest::qWait(2800); // past the exit and the exit grace.
        QCOMPARE(launcher.state(), AppLauncher::State::Failed);
    }

    // Steam's hidden windows come and go while the grown-up screen is up;
    // none of them is the game, so the screen stays.
    void aHiddenWindowAfterNothingCameKeepsTheGrownUpScreen() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("h0"), QStringLiteral("steam"),
                              QStringLiteral("Steam"), true);
        launcher.setLaunchGraceMilliseconds(100);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.windowOpened(QStringLiteral("h1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.windowChanged(QStringLiteral("h1"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.windowClosed(QStringLiteral("h1"));
        launcher.windowClosed(QStringLiteral("h0"));
        QCOMPARE(launcher.state(), AppLauncher::State::Failed);
        QVERIFY(launcher.needsGrownUp());
    }

    // A game that comes late with a splash before its own window is one start,
    // as it would be in time (ADR-0028): the splash going does not end it.
    void aLateGameWithASplashIsOneStart() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.setLaunchGraceMilliseconds(400);
        launcher.launch(QStringLiteral("Pep's Birthday Surprise"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_294700"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowClosed(QStringLiteral("s1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_294700"),
                              QStringLiteral("Pep's Birthday Surprise"));
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        QVERIFY(!launcher.needsGrownUp());
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    void secondLaunchWhileRunningIsIgnored() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Practice"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2")});
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("gcompris"),
                              QStringLiteral("GCompris"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.launch(QStringLiteral("Terminal"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(launcher.title(), QStringLiteral("Practice"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        // Let the launching process exit before the launcher goes out of scope.
        QTest::qWait(400);
    }

    void dismissLeavesComingSoonAndFailed() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Music"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::ComingSoon);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Draw"), {QStringLiteral("/nonexistent/cairn-app")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // A hidden window changing says nothing about the coming-soon screen.
    void comingSoonStaysUntilDismissed() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Music"), {});
        launcher.windowOpened(QStringLiteral("h1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"), true);
        launcher.windowClosed(QStringLiteral("h1"));
        QCOMPARE(launcher.state(), AppLauncher::State::ComingSoon);
    }

    // A window nobody launched, while the tiles are up, is a grown-up's job.
    void windowOnItsOwnInterrupts() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QVERIFY(launcher.needsGrownUp());
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        QCOMPARE(states.count(), 1);
    }

    void windowIsNamedByTitleThenAppIdThenAPlainPhrase_data() {
        QTest::addColumn<QString>("appId");
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("expected");
        QTest::newRow("title") << "steam" << "Steam - Update" << "Steam - Update";
        QTest::newRow("app id") << "steam" << "" << "steam";
        QTest::newRow("nothing") << "" << "" << "Another program";
    }

    void windowIsNamedByTitleThenAppIdThenAPlainPhrase() {
        QFETCH(const QString, appId);
        QFETCH(const QString, title);
        QFETCH(const QString, expected);
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), appId, title);
        QCOMPARE(launcher.title(), expected);
    }

    void ownWindowIsIgnored() {
        AppLauncher launcher(this);
        launcher.setOwnAppId(QStringLiteral("cairn-launcher"));
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("cairn-launcher"),
                              QStringLiteral("Cairn"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    void closingTheWindowReturnsToIdle() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    void everyUnexpectedWindowMustClose() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        launcher.windowOpened(QStringLiteral("w2"), QStringLiteral("steam"),
                              QStringLiteral("Sign in"));
        // The first window keeps the name; the second does not rename the screen.
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        launcher.windowClosed(QStringLiteral("w2"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    void unknownWindowClosingChangesNothing() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.windowClosed(QStringLiteral("never-opened"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QCOMPARE(states.count(), 0);
    }

    // The window a launched program opens is the app, not an interruption,
    // and the launcher tracks the app by that window: it is Running while the
    // window is up and returns to the tiles when it closes (issue #42).
    void aLaunchedWindowIsTrackedUntilItCloses() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2")});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QVERIFY(!launcher.needsGrownUp());
        // The tile keeps its own name; the window does not rename the frame.
        QCOMPARE(launcher.title(), QStringLiteral("Draw"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QTest::qWait(300); // reap the process before the launcher is destroyed.
    }

    // steam -applaunch and flatpak run return within a second while the app
    // runs on; a window that opens after that exit is still the app (issue
    // #42), so the launcher must not return to the tiles when the process
    // ends, only when the window does.
    void aWindowAfterTheLaunchProcessExitsIsStillTheApp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTest::qWait(50); // let the launching process exit, as steam does.
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QVERIFY(!launcher.needsGrownUp());
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    void dismissDoesNotEndAnInterruption() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
    }

    void launchWhileInterruptedIsIgnored() {
        AppLauncher launcher(this);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QCOMPARE(states.count(), 0);
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
    }

    void windowAfterComingSoonTakesOver() {
        AppLauncher launcher(this);
        launcher.launch(QStringLiteral("Draw"), {});
        QCOMPARE(launcher.state(), AppLauncher::State::ComingSoon);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // #99, as it happened in the VM: Steam opened its main window while a game
    // ran, labwc hid it, and the launcher took it for part of the game, so once
    // the game was over the tiles did nothing. A hidden window never counts.
    void aHiddenHelperLeftAfterTheGameDoesNotHoldTheTiles() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        launcher.windowOpened(QStringLiteral("h1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
    }

    // The other half of #99: the same hidden window at the tiles put up a
    // grown-up screen that nothing could clear.
    void aHiddenWindowAtTheTilesDoesNotInterrupt() {
        AppLauncher launcher(this);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.windowOpened(QStringLiteral("h1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QCOMPARE(states.count(), 0);
        // Shown, it is on the screen, and it opened on its own.
        launcher.windowChanged(QStringLiteral("h1"), false);
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QCOMPARE(launcher.title(), QStringLiteral("Steam"));
        launcher.windowChanged(QStringLiteral("h1"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // A window's name is whatever its program chose, so a second window with
    // the game's own app id and title is still not the game's.
    void aSecondWindowIsAGrownUpsJobWhateverItIsCalled() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        launcher.windowOpened(QStringLiteral("g2"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        launcher.windowClosed(QStringLiteral("g2"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(launcher.title(), QStringLiteral("Putt-Putt"));
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QTest::qWait(300); // reap the process before the launcher is destroyed.
    }

    // The compositor says which window a dialog belongs to; that one is the app.
    void aDialogOfTheAppIsTheApp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        launcher.windowOpened(QStringLiteral("d1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Open"), false, QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("d1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // When the program the launcher started has run and then exits, the app
    // is over; a window of it still up after a moment is a grown-up's job.
    void aWindowLeftAfterTheProgramEndsIsAGrownUpsJob() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setSettleMilliseconds(50);
        launcher.launch(QStringLiteral("Story"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.3")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("story"),
                              QStringLiteral("Story"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QTRY_COMPARE_WITH_TIMEOUT(launcher.state(), AppLauncher::State::Interrupted, 5000);
    }

    // The usual end: the window goes with the program, and the moment between
    // the two never flashes the grown-up screen.
    void aWindowThatGoesWithItsProgramNeedsNoGrownUp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setSettleMilliseconds(50);
        launcher.launch(QStringLiteral("Story"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.3")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("story"),
                              QStringLiteral("Story"));
        QTest::qWait(500); // the program has exited; its window is a moment behind.
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QTest::qWait(2500);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    // A program from an earlier launch that hid its window and runs on must
    // not stop the next one starting.
    void aRelaunchWhileAnOldProgramRunsStartsANewOne() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString started = dir.filePath(QStringLiteral("started"));
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 30")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        launcher.windowChanged(QStringLiteral("g1"), true);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("touch '%1'").arg(started)});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_VERIFY(QFile::exists(started));
    }

    // A launch stays Starting until the app's window has stayed on the screen
    // for the steady wait, so the starting screen does not go with a window
    // that is only on its way (ADR-0028).
    void theAppIsOpenOnceItsWindowStaysUp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.launch(QStringLiteral("Putt-Putt"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Joins the Parade"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // As on the first laptop: a game shows a window for a moment, closes it,
    // and opens its own. That is one start, not a game that ended and a
    // window that opened on its own.
    void aSplashBeforeTheGameIsPartOfTheStart() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.setLaunchGraceMilliseconds(5000);
        const QSignalSpy states(&launcher, &AppLauncher::stateChanged);
        launcher.launch(QStringLiteral("Pajama Sam"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTest::qWait(300); // `steam -applaunch` hands off before any window.
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_1"), {});
        QTest::qWait(100);
        launcher.windowClosed(QStringLiteral("s1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_1"),
                              QStringLiteral("Pajama Sam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(launcher.title(), QStringLiteral("Pajama Sam"));
        // Starting, then Running: nothing in between reached the screen.
        QCOMPARE(states.count(), 2);
    }

    // While the app is opening, a second window beside the first is the
    // app's too; a game may open its own before its splash has gone.
    void everyWindowWhileOpeningIsTheApp() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.launch(QStringLiteral("Spy Fox"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_2"), {});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2"),
                              QStringLiteral("Spy Fox"));
        QVERIFY(!launcher.needsGrownUp());
        launcher.windowClosed(QStringLiteral("s1"));
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
    }

    // A window that comes and goes with nothing after it is a launch that did
    // not open: the tiles come back when the grace ends, with no grown-up.
    void aWindowThatComesAndGoesAloneReturnsToTheTiles() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(300);
        launcher.setLaunchGraceMilliseconds(500);
        launcher.launch(QStringLiteral("Pep"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_3"), {});
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    // A window that is up when the grace ends gets the rest of its steady
    // wait; if it then stays, the app is open.
    void aWindowUpAtTheEndOfTheGraceGetsItsWait() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(400);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.launch(QStringLiteral("Freddi Fish"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTest::qWait(100);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Freddi Fish"));
        QTest::qWait(200);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
    }

    // Past the grace, a window that goes before its wait is over ends the
    // launch: nothing is left to wait for.
    void aWindowThatGoesAfterTheGraceEndsTheLaunch() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(5000);
        launcher.setLaunchGraceMilliseconds(100);
        launcher.launch(QStringLiteral("Freddi Fish"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Freddi Fish"));
        QTest::qWait(200);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowClosed(QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    // A window that comes up while the app is opening gets the whole steady
    // wait, even if an earlier window has been up nearly as long.
    void eachNewWindowGetsTheWholeWait() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(400);
        launcher.setLaunchGraceMilliseconds(5000);
        launcher.launch(QStringLiteral("Spy Fox"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_2"), {});
        QTest::qWait(300);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2"),
                              QStringLiteral("Spy Fox"));
        launcher.windowClosed(QStringLiteral("s1"));
        QTest::qWait(200);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
    }

    // The same for a hidden window that is shown.
    void aWindowShownWhileOpeningGetsTheWholeWait() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(400);
        launcher.setLaunchGraceMilliseconds(5000);
        launcher.launch(QStringLiteral("Spy Fox"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_2"), {});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2"),
                              QStringLiteral("Spy Fox"), true);
        QTest::qWait(300);
        launcher.windowChanged(QStringLiteral("g1"), false);
        launcher.windowClosed(QStringLiteral("s1"));
        QTest::qWait(200);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Running);
    }

    // A program that quits after its own window came up is the app ending, as
    // when a child closes it at once: when the window goes, the tiles come
    // back without waiting out the grace.
    void anAppThatQuitsWhileOpeningEndsWithItsWindow() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(5000);
        launcher.setLaunchGraceMilliseconds(15000);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.2; exit 3")});
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        QTest::qWait(600); // the program has quit; its window is a moment behind.
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
    }

    // Right after login the Steam client is still starting and takes a game
    // up after the usual grace; the launch waits for it rather than going
    // back to the tiles and calling its window one that opened on its own
    // (ADR-0029, #132). A stand-in steam on PATH hands off at once, as the
    // real one does.
    void aSteamGameWhileSteamStartsIsWaitedFor() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.setSteamStartedAtLogin(true);
        launcher.launch(
            QStringLiteral("Putt-Putt Goes to the Moon"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294650")});
        QTest::qWait(700);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("scummvm"),
                              QStringLiteral("Putt-Putt Goes to the Moon"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QVERIFY(!launcher.needsGrownUp());
        qputenv("PATH", path);
    }

    // A splash up at the moment a grace ends, and gone before its steady
    // wait, is still part of the start while Steam is working on the game.
    void aSplashAcrossAGraceWhileSteamStartsIsPartOfTheStart() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(3000);
        launcher.setLaunchGraceMilliseconds(300);
        launcher.setSteamStartedAtLogin(true);
        launcher.launch(
            QStringLiteral("Pajama Sam"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294660")});
        QTest::qWait(450);
        launcher.windowOpened(QStringLiteral("s1"), QStringLiteral("steam_app_294660"), {});
        QTest::qWait(400); // a grace ends while the splash is up.
        launcher.windowClosed(QStringLiteral("s1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_294660"),
                              QStringLiteral("Pajama Sam"));
        QVERIFY(!launcher.needsGrownUp());
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        qputenv("PATH", path);
    }

    // A tile that is not a Steam game is not waited for because Steam is
    // starting.
    void anotherTileWhileSteamStartsEndsAtTheGrace() {
        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.setSteamStartedAtLogin(true);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTest::qWait(100);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
    }

    // A Steam game Steam never starts, as when it waits on a dialog the kiosk
    // hides (#139), ends on the grown-up screen naming the tile, not at the
    // tiles where the child presses it again. Steam is up and no reaper runs,
    // so it is not waited for past the grace.
    void aSteamGameThatNeverStartsNeedsAGrownUp() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.launch(
            QStringLiteral("Putt-Putt Enters the Race"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294680")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QVERIFY(launcher.needsGrownUp());
        QCOMPARE(launcher.title(), QStringLiteral("Putt-Putt Enters the Race"));
        qputenv("PATH", path);
    }

    // As on the kids' laptop (2026-10-20): Steam is up, and before the game
    // starts it downloads what the game needs, with no reaper yet. The
    // launch waits past the grace and the starting screen says the game is
    // getting ready, until the game's window comes (ADR-0036).
    void aGameSteamIsGettingReadyIsWaitedFor() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setLaunchGraceMilliseconds(200);
        const QSignalSpy ready(&launcher, &AppLauncher::gettingReadyChanged);
        launcher.launch(
            QStringLiteral("Arthur's Birthday"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("2471140")});
        QVERIFY(!launcher.gettingReady());
        steamSays("2471140", "changed task to DownloadingDepots with \"\"");
        QTRY_VERIFY(launcher.gettingReady());
        QTest::qWait(500);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QVERIFY(launcher.gettingReady());
        QCOMPARE(ready.count(), 1);
        steamSays("2471140", "changed task to Completed with \"\"");
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2471140"),
                              QStringLiteral("Arthur's Birthday"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QVERIFY(!launcher.gettingReady());
        qputenv("PATH", path);
    }

    // Steam got part-way and then asked a question on a window the kiosk
    // hides: the grown-up screen comes at the next grace, as before.
    void aQuestionAfterTheWorkNeedsAGrownUp() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.launch(
            QStringLiteral("Putt-Putt Joins the Circus"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294690")});
        steamSays("294690", "changed task to DownloadingDepots with \"\"");
        QTRY_VERIFY(launcher.gettingReady());
        QTest::ignoreMessage(QtWarningMsg, "Steam is waiting for an answer about Putt-Putt Joins "
                                           "the Circus on a window the kiosk hides.");
        QTest::ignoreMessage(QtWarningMsg,
                             "Nothing of Putt-Putt Joins the Circus came on the screen in time.");
        steamSays("294690", "changed task to SynchronizingCloud with \"\"");
        steamSays("294690", "waiting for user response to SynchronizingCloud \"syncfailed\"");
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QVERIFY(!launcher.gettingReady());
        QCOMPARE(launcher.title(), QStringLiteral("Putt-Putt Joins the Circus"));
        qputenv("PATH", path);
    }

    // Steam refused the launch, as when another game runs: the grown-up
    // screen at the grace, and the journal says Steam refused.
    void aRefusalNeedsAGrownUpAndSaysSo() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        QTest::ignoreMessage(QtWarningMsg, "Steam refused to start Putt-Putt Saves The Zoo.");
        QTest::ignoreMessage(QtWarningMsg,
                             "Nothing of Putt-Putt Saves The Zoo came on the screen in time.");
        launcher.launch(
            QStringLiteral("Putt-Putt Saves The Zoo"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294660")});
        steamSays("294660", "failed with AppError_33 with \"\"");
        steamSays("294660", "changed task to Failed with \"\"");
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QVERIFY(!launcher.gettingReady());
        qputenv("PATH", path);
    }

    // The download ends and Steam, started at login, still has the game in
    // hand: the launch goes on waiting, but the sentence goes at the next
    // grace, since nothing is being got ready any more.
    void theSentenceGoesWhenTheWorkEnds() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.setSteamStartedAtLogin(true);
        launcher.launch(
            QStringLiteral("Stellaluna"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("3586650")});
        steamSays("3586650", "changed task to DownloadingDepots with \"\"");
        QTRY_VERIFY(launcher.gettingReady());
        steamSays("3586650", "changed task to Completed with \"\"");
        QTRY_VERIFY(!launcher.gettingReady());
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        qputenv("PATH", path);
    }

    // Steam was installing an update of itself when the tile was pressed and
    // lost the tap when it restarted, as on the kids' laptop on 2026-10-20:
    // the launch waits, saying the game is getting ready, and once the new
    // client has started without the game, the tap is sent again (ADR-0037).
    void aTapLostWhileSteamUpdatedItselfIsSentAgain() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString runs = dir.filePath(QStringLiteral("runs"));
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\necho \"$@\" >> '" + runs.toUtf8() + "'\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        const auto timesRun = [&runs] {
            QFile file(runs);
            return file.open(QIODevice::ReadOnly) ? file.readAll().count('\n') : 0;
        };

        steamLogs(QStringLiteral("bootstrap_log.txt"), 16,
                  "Startup - updater built Sep  3 2026 01:31:48");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 16, "Found pending update");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 16, "Installing update...");
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.launch(
            QStringLiteral("Arthur's Reading Race"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("2471160")});
        QTRY_COMPARE(timesRun(), 1);
        QTRY_VERIFY(launcher.gettingReady());

        steamLogs(QStringLiteral("bootstrap_log.txt"), 5, "Update complete, launching Steam...");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 5, "Shutdown");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 5,
                  "Startup - updater built Oct 13 2026 18:44:25");
        steamLogs(QStringLiteral("console_log.txt"), 4, "System startup time: 16.79 seconds");
        QTest::qWait(450);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QVERIFY(launcher.gettingReady());
        QCOMPARE(timesRun(), 1);
        steamLogs(QStringLiteral("console_log.txt"), 4, steamStartsItself());
        QTRY_COMPARE(timesRun(), 2);
        // Asked again: the screen waits, saying so, until Steam takes it.
        QTest::qWait(450);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QVERIFY(launcher.gettingReady());
        steamLogs(QStringLiteral("console_log.txt"), 0,
                  "ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' '-applaunch' "
                  "'2471160'\"");
        steamSays("2471160", "changed task to Completed with \"\"");
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2471160"),
                              QStringLiteral("Arthur's Reading Race"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QTest::qWait(500);
        QCOMPARE(timesRun(), 2);
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/bootstrap_log.txt"))));
        qputenv("PATH", path);
    }

    // The game's window has been up: Steam had the tap, whatever its log says,
    // so it is never sent again.
    void aGameWhoseWindowCameIsNeverSentAgain() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString runs = dir.filePath(QStringLiteral("runs"));
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\necho \"$@\" >> '" + runs.toUtf8() + "'\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        const auto timesRun = [&runs] {
            QFile file(runs);
            return file.open(QIODevice::ReadOnly) ? file.readAll().count('\n') : 0;
        };

        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(5000);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.launch(
            QStringLiteral("Arthur's Reading Race"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("2471160")});
        QTRY_COMPARE(timesRun(), 1);
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_2471160"),
                              QStringLiteral("Arthur's Reading Race"));
        steamLogs(QStringLiteral("console_log.txt"), 10, steamStartsItself());
        QTest::qWait(600);
        QCOMPARE(timesRun(), 1);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.giveUp();
        qputenv("PATH", path);
    }

    // #183: a child logged in on the kids' laptop on 2026-10-27 and, before
    // any tap, Steam installed an update of itself and downloaded another.
    // Its updater's windows came up: the tiles stay, and each window is
    // asked to be hidden, so its Cancel button is out of the child's reach.
    void steamUpdatingItselfAtLoginLeavesTheTiles() {
        // A client of its own: the earlier cases' clients had started.
        QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt")));
        steamLogs(QStringLiteral("bootstrap_log.txt"), 40,
                  "Startup - updater built Sep  3 2026 01:31:48");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 40, "Found pending update");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 40, "Installing update...");
        AppLauncher launcher(this);
        launcher.setSteamStartedAtLogin(true);
        const QSignalSpy toHide(&launcher, &AppLauncher::windowToHide);
        launcher.windowOpened(QStringLiteral("u1"), QStringLiteral("steam"),
                              QStringLiteral("Progress"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
        QCOMPARE(toHide.count(), 1);
        QCOMPARE(toHide.at(0).at(0).toString(), QStringLiteral("u1"));
        launcher.windowChanged(QStringLiteral("u1"), true);

        // Restarted to check itself, then started again and downloading.
        steamLogs(QStringLiteral("bootstrap_log.txt"), 20, "Update complete, launching Steam...");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 20, "Shutdown");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 20,
                  "Startup - updater built Oct 21 2026 21:09:45");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 20, "Verifying installation...");
        launcher.windowOpened(QStringLiteral("u2"), QStringLiteral("steam"),
                              QStringLiteral("Steam setup"));
        steamLogs(QStringLiteral("bootstrap_log.txt"), 2,
                  "Startup - updater built Oct 21 2026 21:09:45");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 1,
                  "Downloading update (22,269 of 424,455 KB)...");
        launcher.windowOpened(QStringLiteral("u3"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QCOMPARE(toHide.count(), 3);
        // Steam shows a hidden one again: it is asked again.
        launcher.windowChanged(QStringLiteral("u1"), false);
        QCOMPARE(toHide.count(), 4);
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);

        // The update over and the client started, a window opened on its own.
        steamLogs(QStringLiteral("console_log.txt"), 0, steamStartsItself());
        QTest::ignoreMessage(QtWarningMsg, "Sign in to Steam opened a window on its own.");
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steamwebhelper"),
                              QStringLiteral("Sign in to Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QCOMPARE(toHide.count(), 4);
        launcher.windowClosed(QStringLiteral("w1"));
        for (const QString& window :
             {QStringLiteral("u1"), QStringLiteral("u2"), QStringLiteral("u3")}) {
            launcher.windowClosed(window);
        }
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/bootstrap_log.txt"))));
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt"))));
    }

    // A game tapped while Steam updates itself: the updater's window is not
    // the game's, so the starting screen stays and says the game is getting
    // ready (ADR-0037), and the window is asked to be hidden.
    void steamsUpdaterWindowDuringASteamLaunchKeepsTheStartingScreen() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt")));
        steamLogs(QStringLiteral("bootstrap_log.txt"), 10,
                  "Startup - updater built Sep  3 2026 01:31:48");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 10, "Found pending update");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 10, "Installing update...");
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setLaunchGraceMilliseconds(200);
        const QSignalSpy toHide(&launcher, &AppLauncher::windowToHide);
        launcher.launch(
            QStringLiteral("Arthur's Reading Race"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("2471160")});
        launcher.windowOpened(QStringLiteral("u1"), QStringLiteral("steam"),
                              QStringLiteral("Progress"));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QCOMPARE(toHide.count(), 1);
        QTRY_VERIFY(launcher.gettingReady());
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        launcher.giveUp();
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/bootstrap_log.txt"))));
        qputenv("PATH", path);
    }

    // Another tile tapped while Steam updates itself: a program that is
    // opening owns every window on the screen, and once it is open its new
    // windows are decided as before, so none of its own is ever hidden.
    void anotherAppsWindowWhileSteamUpdatesItselfIsTheApp() {
        QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt")));
        steamLogs(QStringLiteral("bootstrap_log.txt"), 10,
                  "Startup - updater built Sep  3 2026 01:31:48");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 10, "Found pending update");
        steamLogs(QStringLiteral("bootstrap_log.txt"), 10, "Installing update...");
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        const QSignalSpy toHide(&launcher, &AppLauncher::windowToHide);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("sleep 0.3")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Tux Paint"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(toHide.count(), 0);
        // Once it is open, its dialog is the app's and stays on the screen.
        launcher.windowOpened(QStringLiteral("g2"), QStringLiteral("tuxpaint"),
                              QStringLiteral("Save changes?"), false, QStringLiteral("g1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QCOMPARE(toHide.count(), 0);
        launcher.windowClosed(QStringLiteral("g2"));
        // A window with no parent may be the app's too, so it is not hidden:
        // the grown-up screen comes up, as for any window on its own.
        QTest::ignoreMessage(QtWarningMsg, "Open opened a window on its own.");
        launcher.windowOpened(QStringLiteral("g3"), QStringLiteral("xdg-desktop-portal-kde"),
                              QStringLiteral("Open"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QCOMPARE(toHide.count(), 0);
        launcher.giveUp();
        // Its program ends by itself, before the launcher goes.
        QTest::qWait(500);
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/bootstrap_log.txt"))));
    }

    // #182: Steam, starting at login, had its saved sign-in turned down. The
    // starting screen said the game was getting ready while Steam started;
    // the refusal ends the wait at the next grace, and the journal says why.
    void aSteamThatCannotSignInNeedsAGrownUpAndSaysSo() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        // A client of its own: the earlier cases' clients had started.
        QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt")));
        steamLogs(QStringLiteral("bootstrap_log.txt"), 30,
                  "Startup - updater built Oct 13 2026 18:44:25");
        steamLogs(QStringLiteral("console_log.txt"), 28, "Client version: 1792618285");
        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.launch(
            QStringLiteral("Putt-Putt Travels Through Time"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294670")});
        QTRY_VERIFY(launcher.gettingReady());
        QTest::ignoreMessage(QtWarningMsg,
                             "Steam could not sign in, so Putt-Putt Travels Through Time did not "
                             "start. Steam needs to be signed in again.");
        QTest::ignoreMessage(
            QtWarningMsg, "Nothing of Putt-Putt Travels Through Time came on the screen in time.");
        steamLogs(QStringLiteral("console_log.txt"), 0, "LogonFailure Access Denied");
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QVERIFY(!launcher.gettingReady());
        QCOMPARE(launcher.title(), QStringLiteral("Putt-Putt Travels Through Time"));
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/bootstrap_log.txt"))));
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt"))));
        qputenv("PATH", path);
    }

    // #182: Steam, starting at login, cannot reach its servers. The starting
    // screen never says the game is getting ready; after the short wait the
    // grown-up screen comes, and the journal says why.
    void aSteamWithNoConnectionNeedsAGrownUpAndSaysSo() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile fake(dir.filePath(QStringLiteral("steam")));
        QVERIFY(fake.open(QIODevice::WriteOnly));
        fake.write("#!/bin/sh\nexit 0\n");
        fake.close();
        QVERIFY(fake.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);

        QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt")));
        steamLogs(QStringLiteral("bootstrap_log.txt"), 30,
                  "Startup - updater built Oct 13 2026 18:44:25");
        steamLogs(QStringLiteral("console_log.txt"), 28, "Client version: 1792618285");
        steamLogs(QStringLiteral("console_log.txt"), 10, "LogonFailure No Connection");
        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        launcher.setSteamOfflineLimitMilliseconds(500);
        QTest::ignoreMessage(
            QtWarningMsg,
            "Steam could not reach the internet, so Putt-Putt Travels Through Time did not start.");
        QTest::ignoreMessage(
            QtWarningMsg, "Nothing of Putt-Putt Travels Through Time came on the screen in time.");
        launcher.launch(
            QStringLiteral("Putt-Putt Travels Through Time"),
            {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294670")});
        // Still waiting after the first grace, inside the short wait.
        QTest::qWait(300);
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QVERIFY(!launcher.gettingReady());
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QVERIFY(!launcher.gettingReady());
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/bootstrap_log.txt"))));
        QVERIFY(QFile::remove(dataHome().filePath(QStringLiteral("Steam/logs/console_log.txt"))));
        qputenv("PATH", path);
    }

    // #206, acceptance 1: a new child's first tap on a Linux game, and
    // Steam stops part-way. The launcher starts Steam again once and sends
    // the tap again once; the child sees "Getting it ready" until the game.
    void aFirstGameSteamStopsOnIsMendedOnce() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        installStandIns(dir, "active");
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        const QString runs = dir.filePath(QStringLiteral("runs"));
        const QString started = dir.filePath(QStringLiteral("started"));
        forgetSteamsLogs();

        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.setLaunchGraceMilliseconds(200);
        const QSignalSpy ready(&launcher, &AppLauncher::gettingReadyChanged);
        const QSignalSpy restarted(restartOf(launcher), &SteamRestart::finished);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped getting Pajama Sam 2 ready; starting Steam "
                                        "again.");
        QTest::ignoreMessage(QtInfoMsg, "Steam is getting Pajama Sam 2 ready before it starts.");
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped and was started again.");
        QTest::ignoreMessage(QtInfoMsg,
                             "Steam started again without taking Pajama Sam 2; asking again.");
        launcher.launch(QStringLiteral("Pajama Sam 2"), pajamaSam2());
        steamStopsPartWay(70);
        QTRY_COMPARE(linesOf(runs).count(QStringLiteral("-shutdown")), 1);
        // The new client's lines count once Steam has been started again.
        QTRY_COMPARE(restarted.count(), 1);
        QVERIFY(QFileInfo::exists(started));
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
        QVERIFY(launcher.gettingReady());
        steamLogs(QStringLiteral("console_log.txt"), 5, steamStartsItself());
        QTRY_COMPARE(linesOf(runs).count(QStringLiteral("-applaunch 292780")), 2);
        QVERIFY(launcher.gettingReady());
        steamLogs(QStringLiteral("console_log.txt"), 0, pajamaSam2Taken());
        steamSays("292780", "changed task to Completed with \"\"");
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("steam_app_292780"),
                              QStringLiteral("Pajama Sam 2"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        QTest::qWait(600);
        QCOMPARE(linesOf(runs).count(QStringLiteral("-applaunch 292780")), 2);
        QCOMPARE(linesOf(runs).count(QStringLiteral("-shutdown")), 1);
        QCOMPARE(linesOf(started), SteamAtLogin::command().mid(1));
        // "Getting it ready" from the first look until the game.
        QCOMPARE(ready.count(), 2);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::None);
        launcher.giveUp();
        forgetSteamsLogs();
        qputenv("PATH", path);
    }

    // Started again, Steam takes the tap and stops the same way: no second
    // restart, and the grown-up screen comes at the next look.
    void aStallThatComesBackSaysToLogOut() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        installStandIns(dir, "active");
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        const QString runs = dir.filePath(QStringLiteral("runs"));
        forgetSteamsLogs();

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        const QSignalSpy restarted(restartOf(launcher), &SteamRestart::finished);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped getting Pajama Sam 2 ready; starting Steam "
                                        "again.");
        QTest::ignoreMessage(QtInfoMsg, "Steam is getting Pajama Sam 2 ready before it starts.");
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped and was started again.");
        QTest::ignoreMessage(QtInfoMsg,
                             "Steam started again without taking Pajama Sam 2; asking again.");
        QTest::ignoreMessage(QtWarningMsg,
                             "Steam stopped getting Pajama Sam 2 ready, so Pajama Sam 2 did not "
                             "start. Log the child out and back in, then try it again.");
        QTest::ignoreMessage(QtWarningMsg, "Nothing of Pajama Sam 2 came on the screen in time.");
        launcher.launch(QStringLiteral("Pajama Sam 2"), pajamaSam2());
        steamStopsPartWay(70);
        QTRY_COMPARE(restarted.count(), 1);
        steamLogs(QStringLiteral("console_log.txt"), 5, steamStartsItself());
        QTRY_COMPARE(linesOf(runs).count(QStringLiteral("-applaunch 292780")), 2);
        steamLogs(QStringLiteral("console_log.txt"), 0, pajamaSam2Taken());
        steamSays("292780", "changed task to DownloadingDepots with \"\"");
        steamLogs(QStringLiteral("content_log.txt"), 65,
                  "AppID 292780 scheduler finished : removed from schedule (result Suspended, "
                  "state 0xc) ");
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::LogOutAndBackIn);
        QCOMPARE(linesOf(runs).count(QStringLiteral("-shutdown")), 1);
        QCOMPARE(linesOf(dir.filePath(QStringLiteral("started"))).count(QStringLiteral("--user")),
                 1);
        launcher.dismiss();
        forgetSteamsLogs();
        qputenv("PATH", path);
    }

    // Started again, Steam takes the tap and gets the game ready, but no
    // window comes: the grown-up screen still says to log out, and the
    // journal says starting Steam again did not bring the game.
    void aRestartThatBringsNoGameSaysToLogOut() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        installStandIns(dir, "active");
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        const QString runs = dir.filePath(QStringLiteral("runs"));
        forgetSteamsLogs();

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        const QSignalSpy restarted(restartOf(launcher), &SteamRestart::finished);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped getting Pajama Sam 2 ready; starting Steam "
                                        "again.");
        QTest::ignoreMessage(QtInfoMsg, "Steam is getting Pajama Sam 2 ready before it starts.");
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped and was started again.");
        QTest::ignoreMessage(QtInfoMsg,
                             "Steam started again without taking Pajama Sam 2; asking again.");
        QTest::ignoreMessage(QtWarningMsg, "Starting Steam again did not bring Pajama Sam 2. Log "
                                           "the child out and back in, then try it again.");
        QTest::ignoreMessage(QtWarningMsg, "Nothing of Pajama Sam 2 came on the screen in time.");
        launcher.launch(QStringLiteral("Pajama Sam 2"), pajamaSam2());
        steamStopsPartWay(70);
        QTRY_COMPARE(restarted.count(), 1);
        steamLogs(QStringLiteral("console_log.txt"), 5, steamStartsItself());
        QTRY_COMPARE(linesOf(runs).count(QStringLiteral("-applaunch 292780")), 2);
        steamLogs(QStringLiteral("console_log.txt"), 0, pajamaSam2Taken());
        steamSays("292780", "changed task to DownloadingDepots with \"\"");
        steamFetchesSoldier();
        steamSays("292780", "changed task to RunningInstallScript with \"\"");
        steamSays("292780", "changed task to WaitingGameWindow with \"\"");
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::LogOutAndBackIn);
        QCOMPARE(linesOf(runs).count(QStringLiteral("-shutdown")), 1);
        QCOMPARE(linesOf(runs).count(QStringLiteral("-applaunch 292780")), 2);
        launcher.dismiss();
        forgetSteamsLogs();
        qputenv("PATH", path);
    }

    // A Steam the launcher did not start as cairn-steam is not stopped; the
    // grown-up screen comes at the next look.
    void aStallTheLauncherCannotMendSaysToLogOut() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        installStandIns(dir, "inactive");
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        forgetSteamsLogs();

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped getting Pajama Sam 2 ready; starting Steam "
                                        "again.");
        QTest::ignoreMessage(QtInfoMsg, "Steam is getting Pajama Sam 2 ready before it starts.");
        QTest::ignoreMessage(
            QtWarningMsg,
            "Steam is not running as the launcher started it, so it was not started again.");
        QTest::ignoreMessage(QtWarningMsg,
                             "Steam stopped getting Pajama Sam 2 ready, so Pajama Sam 2 did not "
                             "start. Log the child out and back in, then try it again.");
        QTest::ignoreMessage(QtWarningMsg, "Nothing of Pajama Sam 2 came on the screen in time.");
        launcher.launch(QStringLiteral("Pajama Sam 2"), pajamaSam2());
        steamStopsPartWay(70);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::LogOutAndBackIn);
        QCOMPARE(linesOf(dir.filePath(QStringLiteral("runs"))).count(QStringLiteral("-shutdown")),
                 0);
        QVERIFY(!QFileInfo::exists(dir.filePath(QStringLiteral("started"))));
        launcher.dismiss();
        forgetSteamsLogs();
        qputenv("PATH", path);
    }

    // Steam keeps a stuck launch and takes a later tap on the game, writing
    // no step for it, only the tap's own line. That tap ends at its first
    // look, and Steam is not started again for it.
    void aStuckGameTappedAgainStillSaysToLogOut() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        installStandIns(dir, "inactive");
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        forgetSteamsLogs();

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped getting Pajama Sam 2 ready; starting Steam "
                                        "again.");
        QTest::ignoreMessage(QtInfoMsg, "Steam is getting Pajama Sam 2 ready before it starts.");
        QTest::ignoreMessage(
            QtWarningMsg,
            "Steam is not running as the launcher started it, so it was not started again.");
        for (int note = 0; note < 2; ++note) {
            QTest::ignoreMessage(QtWarningMsg,
                                 "Steam stopped getting Pajama Sam 2 ready, so Pajama Sam 2 did "
                                 "not start. Log the child out and back in, then try it again.");
            QTest::ignoreMessage(QtWarningMsg,
                                 "Nothing of Pajama Sam 2 came on the screen in time.");
        }
        launcher.launch(QStringLiteral("Pajama Sam 2"), pajamaSam2());
        steamStopsPartWay(70);
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::LogOutAndBackIn);
        launcher.dismiss();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::None);

        QElapsedTimer sinceTap;
        sinceTap.start();
        launcher.launch(QStringLiteral("Pajama Sam 2"), pajamaSam2());
        steamLogs(QStringLiteral("console_log.txt"), 0, pajamaSam2Taken());
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        // At the first look, not after a wait.
        QVERIFY(sinceTap.elapsed() < 2000);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::LogOutAndBackIn);
        QCOMPARE(linesOf(dir.filePath(QStringLiteral("runs"))).count(QStringLiteral("-shutdown")),
                 0);
        launcher.dismiss();
        forgetSteamsLogs();
        qputenv("PATH", path);
    }

    // Started again, Steam's saved sign-in is turned down: the journal says
    // that, not the stall's advice.
    void aSignInRefusedAfterTheRestartSaysSo() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        installStandIns(dir, "active");
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        forgetSteamsLogs();

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        const QSignalSpy restarted(restartOf(launcher), &SteamRestart::finished);
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped getting Pajama Sam 2 ready; starting Steam "
                                        "again.");
        QTest::ignoreMessage(QtInfoMsg, "Steam is getting Pajama Sam 2 ready before it starts.");
        QTest::ignoreMessage(QtInfoMsg, "Steam stopped and was started again.");
        QTest::ignoreMessage(QtWarningMsg, "Steam could not sign in, so Pajama Sam 2 did not "
                                           "start. Steam needs to be signed in again.");
        QTest::ignoreMessage(QtWarningMsg, "Nothing of Pajama Sam 2 came on the screen in time.");
        launcher.launch(QStringLiteral("Pajama Sam 2"), pajamaSam2());
        steamStopsPartWay(70);
        QTRY_COMPARE(restarted.count(), 1);
        steamLogs(QStringLiteral("console_log.txt"), 0, "LogonFailure Access Denied");
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::None);
        launcher.dismiss();
        forgetSteamsLogs();
        qputenv("PATH", path);
    }

    // The line is only for a launch Steam stopped on part-way (ADR-0045):
    // never for another failure, nor for a window that opened on its own.
    void aFailedLaunchWithoutAStallAddsNoLine() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        installStandIns(dir, "active");
        const QByteArray path = qgetenv("PATH");
        qputenv("PATH", dir.path().toUtf8() + ':' + path);
        forgetSteamsLogs();

        AppLauncher launcher(this);
        launcher.setLaunchGraceMilliseconds(200);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::None);
        QTest::ignoreMessage(QtWarningMsg, "Nothing of Draw came on the screen in time.");
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::None);
        launcher.dismiss();

        QTest::ignoreMessage(QtWarningMsg, "Nothing of Pajama Sam 2 came on the screen in time.");
        launcher.launch(QStringLiteral("Pajama Sam 2"), pajamaSam2());
        QTRY_COMPARE(launcher.state(), AppLauncher::State::Failed);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::None);
        launcher.dismiss();

        launcher.setSteadyMilliseconds(0);
        QTest::ignoreMessage(QtWarningMsg, "Steam opened a window on its own.");
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        QCOMPARE(launcher.grownUpStep(), AppLauncher::GrownUpStep::None);
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(linesOf(dir.filePath(QStringLiteral("runs"))).count(QStringLiteral("-shutdown")),
                 0);
        forgetSteamsLogs();
        qputenv("PATH", path);
    }

    // The grown-up's key always brings the tiles back, whatever is still open.
    void giveUpAlwaysBringsTheTilesBack() {
        AppLauncher launcher(this);
        launcher.setSteadyMilliseconds(0);
        launcher.windowOpened(QStringLiteral("w1"), QStringLiteral("steam"),
                              QStringLiteral("Steam"));
        QCOMPARE(launcher.state(), AppLauncher::State::Interrupted);
        launcher.giveUp();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        QVERIFY(!launcher.needsGrownUp());
        launcher.windowClosed(QStringLiteral("w1"));
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);

        launcher.launch(QStringLiteral("Practice"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        launcher.windowOpened(QStringLiteral("g1"), QStringLiteral("gcompris"),
                              QStringLiteral("GCompris"));
        QCOMPARE(launcher.state(), AppLauncher::State::Running);
        launcher.giveUp();
        QCOMPARE(launcher.state(), AppLauncher::State::Idle);
        launcher.launch(QStringLiteral("Draw"),
                        {shell(), QStringLiteral("-c"), QStringLiteral("exit 0")});
        QCOMPARE(launcher.state(), AppLauncher::State::Starting);
    }
};

} // namespace

QTEST_GUILESS_MAIN(AppLauncherTest)
#include "tst_applauncher.moc"
