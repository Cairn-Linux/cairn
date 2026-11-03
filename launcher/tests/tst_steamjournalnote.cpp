// SPDX-License-Identifier: Apache-2.0
#include "SteamJournalNote.h"

#include <QTest>

namespace {

using SignIn = SteamSignIn::Now;
using Step = SteamLaunchLog::Step;

QString pajamaSam2() {
    return QStringLiteral("Pajama Sam 2");
}

// A sentence SteamStall might give, standing in for any of its own.
QString aStallNote() {
    return QStringLiteral("Steam stopped getting Pajama Sam 2 ready, so it did not start.");
}

// The journal's sentence for a Steam game that put nothing on the screen:
// the reason first, and a grown-up's next step where one is known. What
// Steam's logs must say for each is tst_steamgamewait's to test.
class SteamJournalNoteTest : public QObject {
    Q_OBJECT

private slots:
    // Nothing else helps until a grown-up signs Steam in again, so a refused
    // sign-in comes before a question, a refusal and a stall (ADR-0049).
    void aRefusedSignInComesFirst() {
        const QString note = SteamJournalNote::of(pajamaSam2(), {.signInRefused = true,
                                                                 .lastStep = Step::WaitingForAnswer,
                                                                 .logOutFreesHeldGame = true,
                                                                 .stallNote = aStallNote()});
        QCOMPARE(note, QStringLiteral("Steam could not sign in, so Pajama Sam 2 did not start. "
                                      "Log the child out, then open Steam for them from your "
                                      "own login and sign in again."));
    }

    void aSteamThatCannotReachItsServersSaysSo() {
        const QString note = SteamJournalNote::of(
            pajamaSam2(), {.signIn = SignIn::NoConnection, .stallNote = aStallNote()});
        QCOMPARE(note, QStringLiteral("Steam could not reach the internet, so Pajama Sam 2 did not "
                                      "start."));
    }

    void aQuestionOnAHiddenWindowSaysSteamIsWaiting() {
        const QString note = SteamJournalNote::of(
            pajamaSam2(), {.lastStep = Step::WaitingForAnswer, .stallNote = {}});
        QCOMPARE(note, QStringLiteral("Steam is waiting for an answer about Pajama Sam 2 on a "
                                      "window the kiosk hides."));
    }

    // The cloud question, once Steam has signed in again (#211).
    void aQuestionThatLoggingOutFreesSaysWhatToDo() {
        const QString note = SteamJournalNote::of(
            pajamaSam2(),
            {.lastStep = Step::WaitingForAnswer, .logOutFreesHeldGame = true, .stallNote = {}});
        QCOMPARE(note, QStringLiteral("Steam is waiting for an answer about Pajama Sam 2 on a "
                                      "window the kiosk hides. Log the child out and back in, "
                                      "then try it again."));
    }

    void aRefusalToStartSaysSo() {
        const QString note = SteamJournalNote::of(
            pajamaSam2(), {.lastStep = Step::Refused, .stallNote = aStallNote()});
        QCOMPARE(note, QStringLiteral("Steam refused to start Pajama Sam 2."));
    }

    // With no other reason, a stall's own sentence (#206), or nothing.
    void otherwiseTheStallSays() {
        QCOMPARE(SteamJournalNote::of(pajamaSam2(), {.stallNote = aStallNote()}), aStallNote());
        QCOMPARE(SteamJournalNote::of(pajamaSam2(), {.lastStep = Step::Working, .stallNote = {}}),
                 QString());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamJournalNoteTest)
#include "tst_steamjournalnote.moc"
