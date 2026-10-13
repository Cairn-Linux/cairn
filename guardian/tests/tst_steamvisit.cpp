// SPDX-License-Identifier: Apache-2.0
#include "SteamVisit.h"

#include <QSignalSpy>
#include <QTest>

namespace {

// The stand-in for pkexec says what the helper would, by scenario.
constexpr auto pkexec = QLatin1StringView(FIXTURES "/pkexec");

class SteamVisitTest : public QObject {
    Q_OBJECT

private slots:
    void startsByAskingForThePassword() {
        SteamVisit visit(pkexec, QStringLiteral("asking"));
        QCOMPARE(visit.state(), SteamVisit::State::NotStarted);
        visit.open(QStringLiteral("ada"));
        QCOMPARE(visit.state(), SteamVisit::State::Asking);
        visit.close();
        QTRY_COMPARE(visit.state(), SteamVisit::State::NotOpened);
    }

    void doneClosesAnOpenVisit() {
        SteamVisit visit(pkexec, QStringLiteral("open"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::Open);
        visit.close();
        QTRY_COMPARE(visit.state(), SteamVisit::State::Closed);
        QVERIFY(visit.refusal().isEmpty());
    }

    // Every line the helper says is a state the window shows, in order.
    void followsTheHelperLineByLine() {
        SteamVisit visit(pkexec, QStringLiteral("waiting"));
        QList<SteamVisit::State> seen;
        connect(&visit, &SteamVisit::stateChanged, this,
                [&seen, &visit] { seen.append(visit.state()); });
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::Open);
        visit.close();
        QTRY_COMPARE(visit.state(), SteamVisit::State::Closed);
        const QList<SteamVisit::State> expected = {
            SteamVisit::State::Asking, SteamVisit::State::Waiting, SteamVisit::State::Open,
            SteamVisit::State::Closing, SteamVisit::State::Closed};
        QCOMPARE(seen, expected);
    }

    void theHelperIsToldWhichChild() {
        SteamVisit visit(pkexec, QStringLiteral("names-child"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::NotOpened);
        QCOMPARE(visit.refusal(), QStringLiteral("ada"));
    }

    void aRefusalKeepsTheHelpersReason() {
        SteamVisit visit(pkexec, QStringLiteral("busy"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::NotOpened);
        QCOMPARE(visit.refusal(), QStringLiteral("busy"));
    }

    void aDismissedBoxIsNoPassword() {
        SteamVisit visit(pkexec, QStringLiteral("dismissed"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::NotOpened);
        QCOMPARE(visit.refusal(), QStringLiteral("dismissed"));
    }

    void aPasswordNotAcceptedIsNotAuthorized() {
        SteamVisit visit(pkexec, QStringLiteral("not-authorized"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::NotOpened);
        QCOMPARE(visit.refusal(), QStringLiteral("not-authorized"));
    }

    void aMissingPkexecDoesNotOpen() {
        SteamVisit visit(QStringLiteral("/nonexistent/pkexec"), QStringLiteral("open"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::NotOpened);
        QCOMPARE(visit.refusal(), QStringLiteral("failed"));
    }

    // Steam or cage ended by itself, and the helper closed the visit.
    void aVisitCanCloseWithoutDone() {
        SteamVisit visit(pkexec, QStringLiteral("steam-quits"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::Closed);
    }

    void aProgramThatWouldNotEndIsStuck() {
        SteamVisit visit(pkexec, QStringLiteral("stuck"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::Open);
        visit.close();
        QTRY_COMPARE(visit.state(), SteamVisit::State::Stuck);
    }

    // A helper that ends without saying the visit closed may have left the
    // child refused at login, so the window says what a stuck close says.
    void aHelperThatVanishesIsStuck() {
        SteamVisit visit(pkexec, QStringLiteral("vanishes"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::Open);
        visit.close();
        QTRY_COMPARE(visit.state(), SteamVisit::State::Stuck);
    }

    void aSecondVisitCanOpenAfterTheFirstEnds() {
        SteamVisit visit(pkexec, QStringLiteral("busy"));
        visit.open(QStringLiteral("ada"));
        QTRY_COMPARE(visit.state(), SteamVisit::State::NotOpened);
        visit.open(QStringLiteral("ada"));
        QCOMPARE(visit.state(), SteamVisit::State::Asking);
        QTRY_COMPARE(visit.state(), SteamVisit::State::NotOpened);
    }

    void openingTwiceAtOnceStartsOneHelper() {
        SteamVisit visit(pkexec, QStringLiteral("open"));
        visit.open(QStringLiteral("ada"));
        visit.open(QStringLiteral("ada"));
        QCOMPARE(visit.findChildren<QProcess*>().size(), 1);
        QTRY_COMPARE(visit.state(), SteamVisit::State::Open);
        visit.close();
        QTRY_COMPARE(visit.state(), SteamVisit::State::Closed);
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamVisitTest)
#include "tst_steamvisit.moc"
