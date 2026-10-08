// SPDX-License-Identifier: Apache-2.0
#include "SteamGameWait.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

// A process directory as /proc would show it, in a folder of the test's own.
void addProcess(const QTemporaryDir& root, const QString& pid, const QByteArray& name,
                const QList<QByteArray>& arguments) {
    QVERIFY(QDir(root.path()).mkpath(pid));
    QFile comm(root.filePath(pid + QStringLiteral("/comm")));
    QVERIFY(comm.open(QIODevice::WriteOnly));
    comm.write(name + '\n');
    QFile cmdline(root.filePath(pid + QStringLiteral("/cmdline")));
    QVERIFY(cmdline.open(QIODevice::WriteOnly));
    for (const QByteArray& argument : arguments) {
        cmdline.write(argument + '\0');
    }
}

QStringList steamGame() {
    return {QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("294650")};
}

// Whether a Steam game is still on its way (ADR-0029, #132).
class SteamGameWaitTest : public QObject {
    Q_OBJECT

private slots:
    void theAppIdComesFromApplaunch_data() {
        QTest::addColumn<QStringList>("exec");
        QTest::addColumn<QString>("appId");
        QTest::newRow("steam") << steamGame() << QStringLiteral("294650");
        QTest::newRow("full path")
            << QStringList{QStringLiteral("/usr/bin/steam"), QStringLiteral("-applaunch"),
                           QStringLiteral("70")}
            << QStringLiteral("70");
        QTest::newRow("not steam")
            << QStringList{QStringLiteral("scummvm"), QStringLiteral("-applaunch"),
                           QStringLiteral("70")}
            << QString();
        QTest::newRow("no app id")
            << QStringList{QStringLiteral("steam"), QStringLiteral("-applaunch")} << QString();
        QTest::newRow("not a number")
            << QStringList{QStringLiteral("steam"), QStringLiteral("-applaunch"),
                           QStringLiteral("x")}
            << QString();
        QTest::newRow("nothing") << QStringList{} << QString();
    }

    void theAppIdComesFromApplaunch() {
        QFETCH(const QStringList, exec);
        QFETCH(const QString, appId);
        QCOMPARE(SteamGameWait::appIdOf(exec), appId);
    }

    // With nothing to say Steam is working on it, a Steam game is not waited
    // for past the usual grace.
    void aSteamGameAloneIsNotWaitedFor() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        QVERIFY(!wait.stillComing());
    }

    void steamStartingAtLoginIsWaitedFor() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.setStartupMilliseconds(200);
        wait.steamStartedAtLogin();
        wait.startLaunch(steamGame());
        QVERIFY(wait.stillComing());
        QTRY_VERIFY(!wait.stillComing());
    }

    // Steam starting says nothing about a tile that is not a Steam game.
    void onlyASteamGameIsWaitedFor() {
        const QTemporaryDir root;
        SteamGameWait wait(root.path());
        wait.steamStartedAtLogin();
        wait.startLaunch({QStringLiteral("tuxpaint")});
        QVERIFY(!wait.stillComing());
    }

    void theGamesReaperIsWaitedFor() {
        const QTemporaryDir root;
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"/home/zelda/.local/share/Steam/ubuntu12_32/reaper", "SteamLaunch",
                    "AppId=294650", "--", "steam-launch-wrapper"});
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        QVERIFY(wait.stillComing());
    }

    // Only Steam's reaper for this game counts, never a command line that
    // only mentions it.
    void anotherProcessIsNotTheReaper_data() {
        QTest::addColumn<QByteArray>("name");
        QTest::addColumn<QList<QByteArray>>("arguments");
        QTest::newRow("another game")
            << QByteArray("reaper") << QList<QByteArray>{"reaper", "SteamLaunch", "AppId=70"};
        QTest::newRow("not a launch")
            << QByteArray("reaper") << QList<QByteArray>{"reaper", "AppId=294650"};
        QTest::newRow("named something else")
            << QByteArray("bash")
            << QList<QByteArray>{"bash", "-c", "reaper SteamLaunch AppId=294650"};
        QTest::newRow("a longer app id")
            << QByteArray("reaper") << QList<QByteArray>{"reaper", "SteamLaunch", "AppId=2946501"};
    }

    void anotherProcessIsNotTheReaper() {
        QFETCH(const QByteArray, name);
        QFETCH(const QList<QByteArray>, arguments);
        const QTemporaryDir root;
        addProcess(root, QStringLiteral("100"), name, arguments);
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        QVERIFY(!wait.stillComing());
    }

    // A reaper that never brings a window does not keep the child waiting
    // for ever.
    void theOuterLimitEndsTheWait() {
        const QTemporaryDir root;
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        SteamGameWait wait(root.path());
        wait.setLimitMilliseconds(200);
        wait.steamStartedAtLogin();
        wait.startLaunch(steamGame());
        QVERIFY(wait.stillComing());
        QTRY_VERIFY(!wait.stillComing());
    }

    // A new press starts the limit again and forgets the last game.
    void eachLaunchStartsAfresh() {
        const QTemporaryDir root;
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        SteamGameWait wait(root.path());
        wait.startLaunch(steamGame());
        QVERIFY(wait.stillComing());
        wait.startLaunch({QStringLiteral("tuxpaint")});
        QVERIFY(!wait.stillComing());
    }

    void nothingLaunchedIsNotWaitedFor() {
        const SteamGameWait wait;
        QVERIFY(!wait.stillComing());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamGameWaitTest)
#include "tst_steamgamewait.moc"
