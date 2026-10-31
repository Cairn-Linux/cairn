// SPDX-License-Identifier: Apache-2.0
#include "SteamReapers.h"

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

// Steam's reapers for a game, from the process list (ADR-0029, #117).
class SteamReapersTest : public QObject {
    Q_OBJECT

private slots:
    void theGamesReapersAreFound() {
        const QTemporaryDir root;
        addProcess(root, QStringLiteral("8447"), "reaper",
                   {"/home/ada/.local/share/Steam/ubuntu12_32/reaper", "SteamLaunch",
                    "AppId=294650", "--", "steam-launch-wrapper"});
        addProcess(root, QStringLiteral("9001"), "reaper",
                   {"reaper", "SteamLaunch", "AppId=294650"});
        const SteamReapers reapers(root.path());
        QStringList found = reapers.of(QStringLiteral("294650"));
        found.sort();
        QCOMPARE(found, (QStringList{QStringLiteral("8447"), QStringLiteral("9001")}));
    }

    // Only Steam's reaper for this game counts, never a command line that
    // only mentions it.
    void anotherProcessIsNotTheReaper_data() {
        QTest::addColumn<QString>("pid");
        QTest::addColumn<QByteArray>("name");
        QTest::addColumn<QList<QByteArray>>("arguments");
        QTest::newRow("another game") << QStringLiteral("100") << QByteArray("reaper")
                                      << QList<QByteArray>{"reaper", "SteamLaunch", "AppId=70"};
        QTest::newRow("not a launch") << QStringLiteral("100") << QByteArray("reaper")
                                      << QList<QByteArray>{"reaper", "AppId=294650"};
        QTest::newRow("named something else")
            << QStringLiteral("100") << QByteArray("bash")
            << QList<QByteArray>{"bash", "-c", "reaper SteamLaunch AppId=294650"};
        QTest::newRow("a longer app id")
            << QStringLiteral("100") << QByteArray("reaper")
            << QList<QByteArray>{"reaper", "SteamLaunch", "AppId=2946501"};
        QTest::newRow("not a process")
            << QStringLiteral("self") << QByteArray("reaper")
            << QList<QByteArray>{"reaper", "SteamLaunch", "AppId=294650"};
    }

    void anotherProcessIsNotTheReaper() {
        QFETCH(const QString, pid);
        QFETCH(const QByteArray, name);
        QFETCH(const QList<QByteArray>, arguments);
        const QTemporaryDir root;
        addProcess(root, pid, name, arguments);
        QVERIFY(SteamReapers(root.path()).of(QStringLiteral("294650")).isEmpty());
    }

    void noGameHasNoReapers() {
        const QTemporaryDir root;
        addProcess(root, QStringLiteral("8447"), "reaper", {"reaper", "SteamLaunch", "AppId="});
        QVERIFY(SteamReapers(root.path()).of({}).isEmpty());
    }

    void noProcessListFindsNothing() {
        QVERIFY(SteamReapers(QStringLiteral("/nonexistent/proc"))
                    .of(QStringLiteral("294650"))
                    .isEmpty());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamReapersTest)
#include "tst_steamreapers.moc"
