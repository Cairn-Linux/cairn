// SPDX-License-Identifier: Apache-2.0
#include "CloseRequest.h"

#include <QTest>

namespace {

using Answer = CloseRequest::Answer;
using State = AppLauncher::State;
using LogOutState = LogOut::State;

class CloseRequestTest : public QObject {
    Q_OBJECT

private slots:
    // The kiosk: the launcher is the session, so a close is one step back,
    // the same step Escape takes on the screen that is showing.
    void inTheKioskACloseIsOneStepBack_data() {
        QTest::addColumn<bool>("terminalOpen");
        QTest::addColumn<State>("launcherState");
        QTest::addColumn<Answer>("expected");
        QTest::newRow("tiles") << false << State::Idle << Answer::Stay;
        QTest::newRow("tiles, app starting") << false << State::Starting << Answer::Stay;
        QTest::newRow("tiles, app running") << false << State::Running << Answer::Stay;
        QTest::newRow("terminal") << true << State::Idle << Answer::LeaveTerminal;
        QTest::newRow("terminal, app starting") << true << State::Starting << Answer::LeaveTerminal;
        QTest::newRow("failed from the tiles") << false << State::Failed << Answer::Dismiss;
        QTest::newRow("failed from the terminal") << true << State::Failed << Answer::Dismiss;
        QTest::newRow("coming soon from the tiles")
            << false << State::ComingSoon << Answer::Dismiss;
        QTest::newRow("coming soon from the terminal")
            << true << State::ComingSoon << Answer::Dismiss;
        QTest::newRow("window on its own") << false << State::Interrupted << Answer::Stay;
        QTest::newRow("window on its own, terminal behind")
            << true << State::Interrupted << Answer::Stay;
    }

    void inTheKioskACloseIsOneStepBack() {
        QFETCH(const bool, terminalOpen);
        QFETCH(const State, launcherState);
        QFETCH(const Answer, expected);
        QCOMPARE(CloseRequest::answer(true, terminalOpen, false, LogOutState::Idle, launcherState),
                 expected);
    }

    // The kiosk never answers Close, whatever is showing: that would end the
    // child's session.
    void theKioskNeverCloses() {
        for (const bool terminalOpen : {false, true}) {
            for (const State state : {State::Idle, State::Starting, State::Running, State::Failed,
                                      State::Interrupted, State::ComingSoon}) {
                for (const bool logOutAsked : {false, true}) {
                    for (const LogOutState logOutState :
                         {LogOutState::Idle, LogOutState::Leaving, LogOutState::DidNotFinish}) {
                        QVERIFY(CloseRequest::answer(true, terminalOpen, logOutAsked, logOutState,
                                                     state) != Answer::Close);
                    }
                }
            }
        }
    }

    // The question before logging out is one step from the tiles, so a
    // close answers it as its Back does. A failed launch in front of it
    // still answers first.
    void inTheKioskACloseAnswersTheLogOutQuestionWithStay() {
        QCOMPARE(CloseRequest::answer(true, false, true, LogOutState::Idle, State::Idle),
                 Answer::StayLoggedIn);
        QCOMPARE(CloseRequest::answer(true, false, true, LogOutState::Idle, State::Running),
                 Answer::StayLoggedIn);
        QCOMPARE(CloseRequest::answer(true, false, true, LogOutState::Idle, State::Failed),
                 Answer::Dismiss);
        QCOMPARE(CloseRequest::answer(true, false, true, LogOutState::Idle, State::Interrupted),
                 Answer::Stay);
    }

    // Once the child has said yes, the session is ending under the screen
    // that says so, and a close changes nothing. When the session is still
    // here after the wait, the question is back and a close answers it as
    // Back does. A failed launch in front of it still answers first.
    void inTheKioskACloseWhileLoggingOutStays() {
        QCOMPARE(CloseRequest::answer(true, false, true, LogOutState::Leaving, State::Idle),
                 Answer::Stay);
        QCOMPARE(CloseRequest::answer(true, false, true, LogOutState::Leaving, State::Running),
                 Answer::Stay);
        QCOMPARE(CloseRequest::answer(true, false, true, LogOutState::DidNotFinish, State::Idle),
                 Answer::StayLoggedIn);
        QCOMPARE(CloseRequest::answer(true, false, true, LogOutState::Leaving, State::Failed),
                 Answer::Dismiss);
    }

    // A windowed launcher is a grown-up's run under Plasma: an ordinary window.
    void aWindowedLauncherCloses() {
        QCOMPARE(CloseRequest::answer(false, false, false, LogOutState::Idle, State::Idle),
                 Answer::Close);
        QCOMPARE(CloseRequest::answer(false, true, false, LogOutState::Idle, State::Idle),
                 Answer::Close);
        QCOMPARE(CloseRequest::answer(false, false, false, LogOutState::Idle, State::Failed),
                 Answer::Close);
        QCOMPARE(CloseRequest::answer(false, false, false, LogOutState::Idle, State::Interrupted),
                 Answer::Close);
        QCOMPARE(CloseRequest::answer(false, false, true, LogOutState::Leaving, State::Idle),
                 Answer::Close);
    }
};

} // namespace

QTEST_GUILESS_MAIN(CloseRequestTest)
#include "tst_closerequest.moc"
