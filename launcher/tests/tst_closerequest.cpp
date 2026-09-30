// SPDX-License-Identifier: Apache-2.0
#include "CloseRequest.h"

#include <QTest>

namespace {

using Answer = CloseRequest::Answer;
using State = AppLauncher::State;

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
        QTest::newRow("window on its own") << false << State::Interrupted << Answer::Stay;
        QTest::newRow("window on its own, terminal behind")
            << true << State::Interrupted << Answer::Stay;
    }

    void inTheKioskACloseIsOneStepBack() {
        QFETCH(const bool, terminalOpen);
        QFETCH(const State, launcherState);
        QFETCH(const Answer, expected);
        QCOMPARE(CloseRequest::answer(true, terminalOpen, launcherState), expected);
    }

    // The kiosk never answers Close, whatever is showing: that would end the
    // child's session.
    void theKioskNeverCloses() {
        for (const bool terminalOpen : {false, true}) {
            for (const State state : {State::Idle, State::Starting, State::Running, State::Failed,
                                      State::Interrupted}) {
                QVERIFY(CloseRequest::answer(true, terminalOpen, state) != Answer::Close);
            }
        }
    }

    // A windowed launcher is a grown-up's run under Plasma: an ordinary window.
    void aWindowedLauncherCloses() {
        QCOMPARE(CloseRequest::answer(false, false, State::Idle), Answer::Close);
        QCOMPARE(CloseRequest::answer(false, true, State::Idle), Answer::Close);
        QCOMPARE(CloseRequest::answer(false, false, State::Failed), Answer::Close);
        QCOMPARE(CloseRequest::answer(false, false, State::Interrupted), Answer::Close);
    }
};

} // namespace

QTEST_GUILESS_MAIN(CloseRequestTest)
#include "tst_closerequest.moc"
