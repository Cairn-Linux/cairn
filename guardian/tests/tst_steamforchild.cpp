// SPDX-License-Identifier: Apache-2.0
#include "SteamForChild.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace {

using Step = SteamForChild::Step;
using Reason = SteamForChild::Reason;

// The stand-ins for pkexec with the helper behind it, and for the
// Guardian's own Steam, which is running when the test says so.
class StandIns {
public:
    explicit StandIns(bool ownSteamRunning) {
        qputenv("CAIRN_TEST_PROC", m_proc.path().toLocal8Bit());
        if (ownSteamRunning) {
            QDir(m_proc.path()).mkdir(QStringLiteral("4242"));
            QFile comm(m_proc.filePath(QStringLiteral("4242/comm")));
            if (comm.open(QIODevice::WriteOnly)) {
                comm.write("steam\n");
            }
        }
    }
    ~StandIns() {
        qunsetenv("CAIRN_TEST_PROC");
    }

    SteamForChild::Setup setup(const QString& scenario) const {
        SteamForChild::Setup setup;
        setup.child = QStringLiteral("nobody-here");
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec");
        setup.helper = scenario;
        setup.uid = getuid();
        setup.ownSteam.steam = QStringLiteral(FIXTURES "/steam");
        setup.ownSteam.atLogin = QStringLiteral(FIXTURES "/bazzite-steam");
        setup.ownSteam.proc = m_proc.path();
        setup.ownSteam.shutdownMilliseconds = 1000;
        return setup;
    }

    bool ownSteamStartedAgain() const {
        return QFile::exists(m_proc.filePath(QStringLiteral("started")));
    }

    // The Guardian's own Steam stops acting on -shutdown, as in #217.
    void ownSteamWillNotQuit() const {
        QFile stubborn(m_proc.filePath(QStringLiteral("stubborn")));
        QVERIFY(stubborn.open(QIODevice::WriteOnly));
    }

    // It has gone after all: quit by hand, or done saving at last.
    void ownSteamGone() const {
        QVERIFY(QDir(m_proc.filePath(QStringLiteral("4242"))).removeRecursively());
    }

private:
    QTemporaryDir m_proc;
};

class SteamForChildTest : public QObject {
    Q_OBJECT

private slots:
    void anAccountWithoutAFullNameIsCalledByItsLoginName() {
        const StandIns standIns(false);
        const SteamForChild visit(standIns.setup(QStringLiteral("open")));
        QCOMPARE(visit.childName(), QStringLiteral("nobody-here"));
        QCOMPARE(visit.step(), Step::Ready);
    }

    void openThenDone() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::Open);
        visit.done();
        QTRY_COMPARE(visit.step(), Step::Closed);
        QCOMPARE(visit.reason(), Reason::None);
        QVERIFY(!visit.ownSteamComingBack());
    }

    void theGuardiansOwnSteamClosesFirstAndComesBack() {
        const StandIns standIns(true);
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        QVERIFY(visit.ownSteamOpen());
        QList<Step> seen;
        connect(&visit, &SteamForChild::stepChanged, this,
                [&seen, &visit] { seen.append(visit.step()); });
        visit.open();
        QCOMPARE(visit.step(), Step::ClosingOwnSteam);
        QTRY_COMPARE(visit.step(), Step::Open);
        QVERIFY(!standIns.ownSteamStartedAgain());
        visit.done();
        QTRY_COMPARE(visit.step(), Step::Closed);
        QVERIFY(visit.ownSteamComingBack());
        QTRY_VERIFY(standIns.ownSteamStartedAgain());
        const QList<Step> expected = {Step::ClosingOwnSteam, Step::Asking, Step::Open,
                                      Step::Closing, Step::Closed};
        QCOMPARE(seen, expected);
    }

    void itComesBackWhenTheChildsSteamDoesNotOpenToo() {
        const StandIns standIns(true);
        SteamForChild visit(standIns.setup(QStringLiteral("dismissed")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::NotOpened);
        QCOMPARE(visit.reason(), Reason::NoPassword);
        QTRY_VERIFY(standIns.ownSteamStartedAgain());
    }

    void theScreenLockClosesTheVisit() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::Open);
        visit.screenLocked();
        QTRY_COMPARE(visit.step(), Step::Closed);
    }

    // Locked while the password box was up: nothing opens, and the window
    // starts over.
    void aLockBeforeTheVisitOpensStartsOver() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("asking")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::Asking);
        visit.screenLocked();
        QTRY_COMPARE(visit.step(), Step::Ready);
    }

    void aLockWhileTheOwnSteamClosesStartsOverAndBringsItBack() {
        const StandIns standIns(true);
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        visit.open();
        QCOMPARE(visit.step(), Step::ClosingOwnSteam);
        visit.screenLocked();
        QTRY_COMPARE(visit.step(), Step::Ready);
        QTRY_VERIFY(standIns.ownSteamStartedAgain());
    }

    void theWindowClosesAtOnceWhenNothingIsOpen() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        QVERIFY(visit.leave());
    }

    // The close button during a visit closes the visit first, and the window
    // after it.
    void closingTheWindowClosesTheVisitFirst() {
        const StandIns standIns(true);
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        const QSignalSpy quit(&visit, &SteamForChild::quitRequested);
        visit.open();
        QTRY_COMPARE(visit.step(), Step::Open);
        QVERIFY(!visit.leave());
        QTRY_COMPARE(visit.step(), Step::Closed);
        QCOMPARE(quit.count(), 1);
        QTRY_VERIFY(standIns.ownSteamStartedAgain());
    }

    void closingTheWindowWhileAskingOpensNothing() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("asking")));
        const QSignalSpy quit(&visit, &SteamForChild::quitRequested);
        visit.open();
        QTRY_COMPARE(visit.step(), Step::Asking);
        QVERIFY(!visit.leave());
        QTRY_COMPARE(quit.count(), 1);
        QCOMPARE(visit.step(), Step::Ready);
    }

    // The Guardian needs to read that a restart is needed, so the window
    // stays.
    void aStuckCloseKeepsTheWindow() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("stuck")));
        const QSignalSpy quit(&visit, &SteamForChild::quitRequested);
        visit.open();
        QTRY_COMPARE(visit.step(), Step::Open);
        QVERIFY(!visit.leave());
        QTRY_COMPARE(visit.step(), Step::Stuck);
        QTest::qWait(100);
        QCOMPARE(quit.count(), 0);
        QVERIFY(visit.leave());
    }

    void doneOnlyMeansSomethingWhileOpen() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("busy")));
        visit.done();
        QCOMPARE(visit.step(), Step::Ready);
        visit.open();
        QTRY_COMPARE(visit.step(), Step::NotOpened);
        visit.done();
        QCOMPARE(visit.step(), Step::NotOpened);
    }

    void tryAgainAfterARefusal() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("logged-in")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::NotOpened);
        QCOMPARE(visit.reason(), Reason::LoggedIn);
        visit.open();
        QVERIFY(visit.step() != Step::NotOpened);
        QTRY_COMPARE(visit.step(), Step::NotOpened);
    }

    // Provisioning holds the list while it copies games (ADR-0033): not a
    // visit, so the window must not say one is open.
    void aCopyOfTheGamesIsItsOwnReason() {
        const StandIns standIns(false);
        SteamForChild visit(standIns.setup(QStringLiteral("copying")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::NotOpened);
        QCOMPARE(visit.reason(), Reason::Copying);
    }

    void anOwnSteamThatWillNotCloseKeepsTheChildsShut() {
        const StandIns standIns(true);
        QFile stubborn(QString::fromLocal8Bit(qgetenv("CAIRN_TEST_PROC")) +
                       QStringLiteral("/stubborn"));
        QVERIFY(stubborn.open(QIODevice::WriteOnly));
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::NotOpened);
        QCOMPARE(visit.reason(), Reason::OwnSteamStillOpen);
    }

    // #217: a Steam that will not close is not said to be coming back.
    // Quit by hand, as the window says, it comes back once the visit of
    // Try again has closed.
    void anOwnSteamQuitByHandComesBackAfterTheNextVisit() {
        const StandIns standIns(true);
        standIns.ownSteamWillNotQuit();
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::NotOpened);
        QVERIFY(!visit.ownSteamComingBack());
        QTest::qWait(200);
        QVERIFY(!standIns.ownSteamStartedAgain());

        standIns.ownSteamGone();
        visit.open();
        QTRY_COMPARE(visit.step(), Step::Open);
        visit.done();
        QTRY_COMPARE(visit.step(), Step::Closed);
        QVERIFY(visit.ownSteamComingBack());
        QTRY_VERIFY(standIns.ownSteamStartedAgain());
    }

    // #217: quit by hand, then the Try again is refused. Steam was closed
    // for a visit that did not happen, so it comes back, and the window
    // says so.
    void aRefusedTryAgainAfterAQuitByHandBringsItBack() {
        const StandIns standIns(true);
        standIns.ownSteamWillNotQuit();
        SteamForChild visit(standIns.setup(QStringLiteral("dismissed")));
        visit.open();
        QTRY_COMPARE(visit.step(), Step::NotOpened);
        QCOMPARE(visit.reason(), Reason::OwnSteamStillOpen);
        standIns.ownSteamGone();
        visit.open();
        QTRY_COMPARE(visit.reason(), Reason::NoPassword);
        QVERIFY(visit.ownSteamComingBack());
        QTRY_VERIFY(standIns.ownSteamStartedAgain());
    }

    // #217: Steam saved for longer than the wait and went after the window
    // said it did not close. Closing the window, rather than trying again,
    // brings it back, as the first screen said.
    void closingTheWindowAfterTheOwnSteamWentLateBringsItBack() {
        const StandIns standIns(true);
        standIns.ownSteamWillNotQuit();
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        visit.open();
        QTRY_COMPARE(visit.reason(), Reason::OwnSteamStillOpen);
        standIns.ownSteamGone();
        QVERIFY(visit.leave());
        QTRY_VERIFY(standIns.ownSteamStartedAgain());
    }

    // While it still runs, closing the window starts nothing.
    void closingTheWindowWhileTheOwnSteamStillRunsStartsNothing() {
        const StandIns standIns(true);
        standIns.ownSteamWillNotQuit();
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        visit.open();
        QTRY_COMPARE(visit.reason(), Reason::OwnSteamStillOpen);
        QVERIFY(visit.leave());
        QTest::qWait(200);
        QVERIFY(!standIns.ownSteamStartedAgain());
    }

    // #217: the close button while a Steam that will not close was being
    // closed. It is still there, so nothing starts it or says it comes back.
    void closingTheWindowWhileTheOwnSteamWillNotCloseStartsNothing() {
        const StandIns standIns(true);
        standIns.ownSteamWillNotQuit();
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        const QSignalSpy quit(&visit, &SteamForChild::quitRequested);
        visit.open();
        QVERIFY(!visit.leave());
        QTRY_COMPARE(quit.count(), 1);
        QVERIFY(!visit.ownSteamComingBack());
        QTest::qWait(200);
        QVERIFY(!standIns.ownSteamStartedAgain());
    }

    // #217: locked while a Steam that will not close was being closed. The
    // window starts over with Steam still there; once Steam has gone,
    // closing the window brings it back.
    void aLockWhileTheOwnSteamWillNotCloseBringsItBackOnceItHasGone() {
        const StandIns standIns(true);
        standIns.ownSteamWillNotQuit();
        SteamForChild visit(standIns.setup(QStringLiteral("open")));
        visit.open();
        visit.screenLocked();
        QTRY_COMPARE(visit.step(), Step::Ready);
        QVERIFY(!visit.ownSteamComingBack());
        QTest::qWait(200);
        QVERIFY(!standIns.ownSteamStartedAgain());
        standIns.ownSteamGone();
        QVERIFY(visit.leave());
        QTRY_VERIFY(standIns.ownSteamStartedAgain());
    }

    void eachWordOfTheHelperHasItsReason() {
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("busy")), Reason::Busy);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("copying")), Reason::Copying);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("logging-out")), Reason::LoggingOut);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("logged-in")), Reason::LoggedIn);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("still-running")), Reason::StillEnding);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("not-set-up")), Reason::NotSetUp);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("not-a-child")), Reason::NotAChild);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("no-desktop")), Reason::NotAtTheComputer);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("not-a-grown-up")), Reason::NotAllowed);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("dismissed")), Reason::NoPassword);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("not-authorized")), Reason::NoPassword);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("usage")), Reason::Other);
        QCOMPARE(SteamForChild::reasonFor(QStringLiteral("failed")), Reason::Other);
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamForChildTest)
#include "tst_steamforchild.moc"
