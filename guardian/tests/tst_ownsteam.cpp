// SPDX-License-Identifier: Apache-2.0
#include "OwnSteam.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace {

// A stand-in process list, with the client as 4242 when it is running. The
// stand-in Steam removes it when asked to shut down.
class StandInProc {
public:
    StandInProc() {
        qputenv("CAIRN_TEST_PROC", m_dir.path().toLocal8Bit());
    }
    ~StandInProc() {
        qunsetenv("CAIRN_TEST_PROC");
    }
    QString path() const {
        return m_dir.path();
    }

    void addProcess(const QString& pid, const QByteArray& name) const {
        QDir(m_dir.path()).mkdir(pid);
        QFile comm(m_dir.filePath(pid + QStringLiteral("/comm")));
        QVERIFY(comm.open(QIODevice::WriteOnly));
        comm.write(name + '\n');
    }

    void makeStubborn() const {
        QFile stubborn(m_dir.filePath(QStringLiteral("stubborn")));
        QVERIFY(stubborn.open(QIODevice::WriteOnly));
    }

    QByteArray started() const {
        QFile started(m_dir.filePath(QStringLiteral("started")));
        return started.open(QIODevice::ReadOnly) ? started.readAll() : QByteArray();
    }

private:
    QTemporaryDir m_dir;
};

OwnSteam::Programs programsFor(const StandInProc& proc) {
    OwnSteam::Programs programs;
    programs.steam = QStringLiteral(FIXTURES "/steam");
    programs.atLogin = QStringLiteral(FIXTURES "/bazzite-steam");
    programs.proc = proc.path();
    programs.shutdownMilliseconds = 1000;
    return programs;
}

class OwnSteamTest : public QObject {
    Q_OBJECT

private slots:
    void aClientOfThisAccountIsRunning() {
        const StandInProc proc;
        proc.addProcess(QStringLiteral("4242"), "steam");
        const OwnSteam steam(getuid(), programsFor(proc));
        QVERIFY(steam.running());
    }

    // Steam's helpers and the scripts that start it are not the client.
    void otherProgramsAreNot() {
        const StandInProc proc;
        proc.addProcess(QStringLiteral("4243"), "steamwebhelper");
        proc.addProcess(QStringLiteral("4244"), "bazzite-steam");
        proc.addProcess(QStringLiteral("self"), "steam");
        const OwnSteam steam(getuid(), programsFor(proc));
        QVERIFY(!steam.running());
    }

    // A child's Steam in a visit runs under the child's account.
    void anotherAccountsClientIsNot() {
        const StandInProc proc;
        proc.addProcess(QStringLiteral("4242"), "steam");
        const OwnSteam steam(getuid() + 1, programsFor(proc));
        QVERIFY(!steam.running());
    }

    void pausingAClosedSteamSaysSoAndStartsNothing() {
        const StandInProc proc;
        OwnSteam steam(getuid(), programsFor(proc));
        const QSignalSpy paused(&steam, &OwnSteam::paused);
        steam.pause();
        QCOMPARE(paused.count(), 0);
        QTRY_COMPARE(paused.count(), 1);
        QCOMPARE(paused.first().first().toBool(), true);
        QVERIFY(!steam.resume());
        QTest::qWait(200);
        QVERIFY(proc.started().isEmpty());
    }

    void pausingAsksSteamToShutDownAndResumingStartsItAgain() {
        const StandInProc proc;
        proc.addProcess(QStringLiteral("4242"), "steam");
        OwnSteam steam(getuid(), programsFor(proc));
        const QSignalSpy paused(&steam, &OwnSteam::paused);
        steam.pause();
        QTRY_COMPARE(paused.count(), 1);
        QCOMPARE(paused.first().first().toBool(), true);
        QVERIFY(!steam.running());
        QVERIFY(steam.resume());
        QTRY_COMPARE(proc.started(), QByteArray("bazzite-steam -silent\n"));
        // Once only.
        QVERIFY(!steam.resume());
    }

    void withoutBazzitesScriptThePlainClientStarts() {
        const StandInProc proc;
        proc.addProcess(QStringLiteral("4242"), "steam");
        OwnSteam::Programs programs = programsFor(proc);
        programs.atLogin = QStringLiteral("/nonexistent/bazzite-steam");
        OwnSteam steam(getuid(), programs);
        const QSignalSpy paused(&steam, &OwnSteam::paused);
        steam.pause();
        QTRY_COMPARE(paused.count(), 1);
        QVERIFY(steam.resume());
        QTRY_COMPARE(proc.started(), QByteArray("steam -silent\n"));
    }

    void aSteamThatWillNotCloseIsSaidToBeOpen() {
        const StandInProc proc;
        proc.addProcess(QStringLiteral("4242"), "steam");
        proc.makeStubborn();
        OwnSteam steam(getuid(), programsFor(proc));
        const QSignalSpy paused(&steam, &OwnSteam::paused);
        steam.pause();
        QTRY_COMPARE(paused.count(), 1);
        QCOMPARE(paused.first().first().toBool(), false);
        QVERIFY(steam.running());
    }

    // #217: Steam had stopped acting on -shutdown. While it still runs,
    // nothing starts it again or says it is coming back.
    void aSteamThatWillNotCloseIsNotStartedAgain() {
        const StandInProc proc;
        proc.addProcess(QStringLiteral("4242"), "steam");
        proc.makeStubborn();
        OwnSteam steam(getuid(), programsFor(proc));
        const QSignalSpy paused(&steam, &OwnSteam::paused);
        steam.pause();
        QTRY_COMPARE(paused.count(), 1);
        QVERIFY(!steam.resume());
        QTest::qWait(200);
        QVERIFY(proc.started().isEmpty());
    }

    // #217: the Guardian quit it from its icon, as the window said, and
    // tried again. It comes back after that visit, as the first screen
    // said it would.
    void aSteamQuitByHandAfterItWouldNotCloseComesBackLater() {
        const StandInProc proc;
        proc.addProcess(QStringLiteral("4242"), "steam");
        proc.makeStubborn();
        OwnSteam steam(getuid(), programsFor(proc));
        const QSignalSpy paused(&steam, &OwnSteam::paused);
        steam.pause();
        QTRY_COMPARE(paused.count(), 1);
        QVERIFY(!steam.resume());
        QVERIFY(QDir(proc.path() + QStringLiteral("/4242")).removeRecursively());
        steam.pause();
        QTRY_COMPARE(paused.count(), 2);
        QCOMPARE(paused.last().first().toBool(), true);
        QVERIFY(steam.resume());
        QTRY_COMPARE(proc.started(), QByteArray("bazzite-steam -silent\n"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(OwnSteamTest)
#include "tst_ownsteam.moc"
