// SPDX-License-Identifier: Apache-2.0
#include "SteamLaunchLog.h"
#include "SteamQuestions.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Step = SteamLaunchLog::Step;

QString pajamaSam2() {
    return QStringLiteral("292780");
}

void append(const QString& path, const QByteArray& text) {
    QFile log(path);
    QVERIFY(log.open(QIODevice::WriteOnly | QIODevice::Append));
    log.write(text);
}

// Steam taking a tap on the game `appId`.
QByteArray tapTaken(const QByteArray& appId) {
    return "[2026-10-31 10:12:04] ExecCommandLine: \"'~/.steam/root/ubuntu12_32/steam' "
           "'-applaunch' '" +
           appId + "'\"\n";
}

// A launch step for the game `appId`, as Steam writes it.
QByteArray step(const QByteArray& says, const QByteArray& appId = "292780") {
    return "[2026-10-31 10:12:05] GameAction [AppID " + appId + ", ActionID 1] : LaunchApp " +
           says + '\n';
}

// The question Steam asks about a game's Steam Cloud saves when it cannot
// reach its servers, as on the test VM (#211).
QByteArray cloudQuestion(const QByteArray& appId = "292780") {
    return step("changed task to SynchronizingCloud with \"\"", appId) +
           step("waiting for user response to SynchronizingCloud \"syncfailed\"", appId);
}

// The question Steam asks before a game's interstitials, as it wrote it on
// the test VM (2026-10-30). Steam answered it itself there, in the same
// second; here nothing answers it.
QByteArray anotherQuestion(const QByteArray& appId = "292780") {
    return step("changed task to ShowInterstitials with \"\"", appId) +
           step("waiting for user response to ShowInterstitials \"\"", appId);
}

// A client's own start command, as the login's `steam -silent` runs it.
QByteArray ownStart() {
    return "[2026-10-31 10:12:03] ExecCommandLine: \"'/var/home/child/.local/share/Steam/"
           "ubuntu12_32/steam' '-srt-logger-opened' '-silent'\"\n";
}

// Steam's launch log in a folder of the test's own, and what reads it.
class Taps {
public:
    // A tap on the game `appId`: the launch log counts from now.
    void tap(const QString& appId) {
        launch.startLaunch(appId);
        questions.startLaunch(appId);
    }

    void console(const QByteArray& lines) const {
        append(path(), lines);
    }

    // Reads what the launch log gained, as SteamGameWait does at the end of
    // each grace, and says where the launch has got to.
    Step read() {
        launch.read();
        return questions.read(launch);
    }

    // A tap on the game `appId` that Steam takes and asks its cloud question
    // about.
    void asked(const QByteArray& appId) {
        tap(QString::fromLatin1(appId));
        console(tapTaken(appId) + cloudQuestion(appId));
        QCOMPARE(read(), Step::WaitingForAnswer);
    }

    // A later tap on the game `appId` that Steam takes and writes nothing
    // more for, and where it has got to.
    Step takenWithoutAStep(const QByteArray& appId) {
        tap(QString::fromLatin1(appId));
        console(tapTaken(appId));
        return read();
    }

    QString path() const {
        return dir.filePath(QStringLiteral("console_log.txt"));
    }

    QTemporaryDir dir;
    SteamLaunchLog launch{path()};
    SteamQuestions questions;
};

// The games Steam holds behind a question on a window the kiosk hides, for
// later taps on them (#211).
class SteamQuestionsTest : public QObject {
    Q_OBJECT

private slots:
    // On the test VM each later tap on Pajama Sam 2 made Steam write only
    // the tap's own line, until the child logged out and back in. Each of
    // them is the question, at every look.
    void aLaterTapOnTheGameIsTheQuestion() {
        Taps taps;
        taps.asked("292780");
        for (int tap = 0; tap < 3; ++tap) {
            taps.tap(pajamaSam2());
            // Steam has not taken this tap yet, as while it updates itself.
            QCOMPARE(taps.read(), Step::Nothing);
            taps.console(tapTaken("292780"));
            QCOMPARE(taps.read(), Step::WaitingForAnswer);
            QCOMPARE(taps.read(), Step::WaitingForAnswer);
        }
    }

    // Steam writes the tap's line alone for an app id it will not launch,
    // such as the test VM's made-up games. Without a question before, that
    // is no question.
    void aTapWithoutAStepIsNoQuestionByItself() {
        Taps taps;
        QCOMPARE(taps.takenWithoutAStep("900001"), Step::Nothing);
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::Nothing);
    }

    // The question holds only its own game: Steam starts the others, and one
    // it takes without a step is no question.
    void anotherGameIsNotHeldByTheQuestion() {
        Taps taps;
        taps.asked("292780");
        QCOMPARE(taps.takenWithoutAStep("900001"), Step::Nothing);
        taps.tap(QStringLiteral("2471140"));
        taps.console(tapTaken("2471140") + step("changed task to Completed with \"\"", "2471140"));
        QCOMPARE(taps.read(), Step::GameStarted);
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::WaitingForAnswer);
    }

    // Offline, each game with cloud saves gets a question of its own, as on
    // the kids' laptop's evening without internet (ADR-0030).
    void eachGameKeepsItsOwnQuestion() {
        Taps taps;
        taps.asked("292780");
        taps.asked("294650");
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::WaitingForAnswer);
        QCOMPARE(taps.takenWithoutAStep("294650"), Step::WaitingForAnswer);
        taps.tap(pajamaSam2());
        taps.console(tapTaken("292780") + step("changed task to SynchronizingStats with \"\""));
        QCOMPARE(taps.read(), Step::Working);
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::Nothing);
        QCOMPARE(taps.takenWithoutAStep("294650"), Step::WaitingForAnswer);
    }

    // Any other step Steam writes for the game means the question is no
    // longer holding it.
    void aStepForTheGameForgetsItsQuestion_data() {
        QTest::addColumn<QByteArray>("says");
        QTest::addColumn<Step>("next");
        QTest::newRow("went on") << QByteArray("changed task to SynchronizingStats with \"\"")
                                 << Step::Working;
        QTest::newRow("refused") << QByteArray("failed with AppError_33 with \"\"")
                                 << Step::Refused;
        QTest::newRow("the game started")
            << QByteArray("changed task to Completed with \"\"") << Step::GameStarted;
    }

    void aStepForTheGameForgetsItsQuestion() {
        QFETCH(const QByteArray, says);
        QFETCH(const Step, next);
        Taps taps;
        taps.asked("292780");
        taps.tap(pajamaSam2());
        taps.console(tapTaken("292780") + step(says));
        QCOMPARE(taps.read(), next);
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::Nothing);
    }

    // Answered while its own tap still waits, the question goes too.
    void aQuestionAnsweredInItsOwnTapIsForgotten() {
        Taps taps;
        taps.asked("292780");
        taps.console(step("changed task to SynchronizingStats with \"\""));
        QCOMPARE(taps.read(), Step::Working);
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::Nothing);
    }

    // A new client holds no question an earlier one asked: one that ran its
    // own start command, one a tap started with the game, or one that began
    // a new log. Every game is freed, not only the one tapped.
    void aNewClientForgetsEveryQuestion_data() {
        QTest::addColumn<QString>("start");
        QTest::newRow("its own start") << QStringLiteral("own start");
        QTest::newRow("started with the tap") << QStringLiteral("with the tap");
        QTest::newRow("a new log") << QStringLiteral("new log");
    }

    void aNewClientForgetsEveryQuestion() {
        QFETCH(const QString, start);
        Taps taps;
        taps.asked("292780");
        taps.asked("294650");
        taps.tap(pajamaSam2());
        if (start == QStringLiteral("own start")) {
            taps.console(ownStart() + tapTaken("292780"));
        } else if (start == QStringLiteral("with the tap")) {
            taps.console("[2026-10-31 10:12:03] ExecCommandLine: \"'/var/home/child/.local/"
                         "share/Steam/ubuntu12_32/steam' '-srt-logger-opened' '-applaunch' "
                         "'292780'\"\n");
        } else {
            QVERIFY(QFile::remove(taps.path()));
            taps.console(tapTaken("292780"));
        }
        QCOMPARE(taps.read(), Step::Nothing);
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::Nothing);
        QCOMPARE(taps.takenWithoutAStep("294650"), Step::Nothing);
    }

    void steamStartedAgainForgetsEveryQuestion() {
        Taps taps;
        taps.asked("292780");
        taps.asked("294650");
        taps.questions.steamRestarted();
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::Nothing);
        QCOMPARE(taps.takenWithoutAStep("294650"), Step::Nothing);
    }

    // A new client asks again while it still cannot reach its servers: the
    // game is held again.
    void aQuestionAskedAgainHoldsTheGameAgain() {
        Taps taps;
        taps.asked("292780");
        taps.tap(pajamaSam2());
        taps.console(ownStart() + tapTaken("292780") + cloudQuestion());
        QCOMPARE(taps.read(), Step::WaitingForAnswer);
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::WaitingForAnswer);
    }

    // Only a later tap that Steam takes on a game its cloud question holds
    // is held by the cloud question: not the tap that met the question, not
    // a tap Steam has not taken yet, not a tap in which a new client asks
    // again, and not a tap on another game, also right after a held one
    // (#211, ADR-0045).
    void aLaterTapOnAGameTheCloudQuestionHoldsIsSeen() {
        Taps taps;
        taps.asked("292780");
        QVERIFY(!taps.questions.heldByCloudQuestion());
        taps.tap(pajamaSam2());
        QCOMPARE(taps.read(), Step::Nothing);
        QVERIFY(!taps.questions.heldByCloudQuestion());
        taps.console(tapTaken("292780"));
        QCOMPARE(taps.read(), Step::WaitingForAnswer);
        QVERIFY(taps.questions.heldByCloudQuestion());
        QCOMPARE(taps.read(), Step::WaitingForAnswer);
        QVERIFY(taps.questions.heldByCloudQuestion());
        // A new client takes the same tap and asks again: this tap met the
        // question after all.
        taps.console(ownStart() + tapTaken("292780") + cloudQuestion());
        QCOMPARE(taps.read(), Step::WaitingForAnswer);
        QVERIFY(!taps.questions.heldByCloudQuestion());
        QCOMPARE(taps.takenWithoutAStep("900001"), Step::Nothing);
        QVERIFY(!taps.questions.heldByCloudQuestion());
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::WaitingForAnswer);
        QVERIFY(taps.questions.heldByCloudQuestion());
        QCOMPARE(taps.takenWithoutAStep("900001"), Step::Nothing);
        QVERIFY(!taps.questions.heldByCloudQuestion());
    }

    // Each game keeps its own question. A game another question holds is
    // held all the same, but not by the cloud question.
    void anotherQuestionIsNotTheCloudQuestion() {
        Taps taps;
        taps.asked("294650");
        taps.tap(pajamaSam2());
        taps.console(tapTaken("292780") + anotherQuestion());
        QCOMPARE(taps.read(), Step::WaitingForAnswer);
        QCOMPARE(taps.takenWithoutAStep("292780"), Step::WaitingForAnswer);
        QVERIFY(!taps.questions.heldByCloudQuestion());
        QCOMPARE(taps.takenWithoutAStep("294650"), Step::WaitingForAnswer);
        QVERIFY(taps.questions.heldByCloudQuestion());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamQuestionsTest)
#include "tst_steamquestions.moc"
