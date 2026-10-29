// SPDX-License-Identifier: Apache-2.0
#include "FinishGames.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QLockFile>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <sys/stat.h>
#include <memory>
#include <unistd.h>

namespace {

// Portal 2, Celeste and Zoo Tycoon are in the shared folder.
constexpr auto sharedGames = R"({"version": 1, "entries": [
  {"id": "steam-620", "title": "Portal 2", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "620"]},
  {"id": "steam-504230", "title": "Celeste", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "504230"]},
  {"id": "steam-12", "title": "Zoo Tycoon", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "12"]}
]}
)";

// ada has Portal 2 and Celeste.
constexpr auto adaHasTwo = R"({"version": 1, "entries": [
  {"id": "steam-620", "title": "Portal 2", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "620"]},
  {"id": "steam-504230", "title": "Celeste", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "504230"]}
]}
)";

QList<Account> family() {
    return {
        {.uid = 1002, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = {}},
        {.uid = 1003, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = {}},
        {.uid = 1001,
         .groups = {"gina", "wheel", "cairn-guardian"},
         .login = "gina",
         .fullName = {}},
    };
}

class FinishGamesTest : public QObject {
    Q_OBJECT

    FinishGames::Outcome run(const QStringList& arguments) const {
        FinishGames::Setup setup;
        setup.euid = m_euid;
        setup.pkexecUid = m_pkexecUid;
        setup.lookUpUid = [](unsigned int uid) -> std::optional<Account> {
            for (const Account& account : family()) {
                if (account.uid == uid) {
                    return account;
                }
            }
            return std::nullopt;
        };
        setup.copy.systemdRun = path("systemd-run");
        setup.copy.visitHelper = path("cairn-steam-visit");
        setup.gamesFile = path("games.json");
        setup.restorecon = path("restorecon");
        setup.gamesFolder = path("games");
        setup.waitingFolder = path("games-waiting");
        setup.finishedFolder = path("games-finished");
        setup.owner = getuid();
        setup.lock = path("lock");
        return FinishGames(setup).run(arguments);
    }

    QString path(const QString& name) const {
        return QDir(m_temporary->path()).filePath(name);
    }

    void give(const QString& name, const QByteArray& text) const {
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(text);
    }

    QByteArray read(const QString& name) const {
        QFile file(path(name));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    QStringList lines(const QString& name) const {
        return QString::fromUtf8(read(name)).split('\n', Qt::SkipEmptyParts);
    }

    void nothingRan() const {
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
        QVERIFY(!QFile::exists(path("calls")));
        QCOMPARE(read("games/ada.json"), QByteArray(adaHasTwo));
        QVERIFY(QDir(path("games-finished")).isEmpty());
    }

    std::unique_ptr<QTemporaryDir> m_temporary;
    unsigned int m_euid = 0;
    std::optional<QString> m_pkexecUid;

private slots:
    void init() {
        m_temporary = std::make_unique<QTemporaryDir>();
        QVERIFY(m_temporary->isValid());
        const QHash<QString, QString> links{{"restorecon", "system-tool"},
                                            {"systemd-run", "systemd-run"},
                                            {"cairn-steam-visit", "copy-helper"}};
        for (auto link = links.cbegin(); link != links.cend(); ++link) {
            QVERIFY(QFile::link(QStringLiteral(FIXTURES "/") + link.value(), path(link.key())));
        }
        for (const char* folder : {"games", "games-waiting", "games-finished", "game-pictures"}) {
            QVERIFY(QDir().mkpath(path(folder)));
        }
        give("games.json", sharedGames);
        give("games/ada.json", adaHasTwo);
        give("game-pictures/620.jpg", "a picture");
        m_euid = 0;
        m_pkexecUid = std::nullopt;
    }

    // pkexec always sets PKEXEC_UID; only systemd starts this.
    void refusesPkexec() {
        give("games-waiting/ada", "ada=620,12\n");
        m_pkexecUid = QStringLiteral("1001");
        QCOMPARE(run({"finish-games", "1002"}).refusal, std::optional(Refusal::NotForPkexec));
        nothingRan();
        QVERIFY(QFile::exists(path("games-waiting/ada")));
    }

    void refusesAnyoneButRoot() {
        give("games-waiting/ada", "ada=620,12\n");
        m_euid = 1002;
        QCOMPARE(run({"finish-games", "1002"}).refusal, std::optional(Refusal::NotRoot));
        nothingRan();
    }

    void refusesWhatIsNotAUid_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("no uid") << QStringList{"finish-games"};
        QTest::newRow("a name") << QStringList{"finish-games", "ada"};
        QTest::newRow("a sign") << QStringList{"finish-games", "-1"};
        QTest::newRow("too long") << QStringList{"finish-games", "10000000000"};
        QTest::newRow("too big") << QStringList{"finish-games", "4294967296"};
        QTest::newRow("one more") << QStringList{"finish-games", "1002", "1003"};
        QTest::newRow("empty") << QStringList{"finish-games", ""};
    }

    void refusesWhatIsNotAUid() {
        QFETCH(const QStringList, arguments);
        give("games-waiting/ada", "ada=620,12\n");
        QCOMPARE(run(arguments).refusal, std::optional(Refusal::Usage));
        nothingRan();
    }

    // Every logout starts it: a Guardian's, an unknown uid's, or a child's
    // with nothing waiting does nothing and says nothing.
    void nothingToDoSaysNothing_data() {
        QTest::addColumn<QString>("uid");
        QTest::newRow("a Guardian") << "1001";
        QTest::newRow("no such account") << "4242";
        QTest::newRow("a child with nothing waiting") << "1003";
    }

    void nothingToDoSaysNothing() {
        QFETCH(const QString, uid);
        give("games-waiting/gina", "gina=620\n");
        const FinishGames::Outcome outcome = run({"finish-games", uid});
        QVERIFY(!outcome.refusal);
        QVERIFY(outcome.notes.isEmpty());
        nothingRan();
    }

    // The waiting list is the child's whole list: the game not on it goes,
    // and only the waiting games not had yet are copied, with no pictures
    // made. How it ended is written down, and the wait is over.
    void copiesTheWaitingGames() {
        give("games-waiting/ada", "ada=620,12\n");
        const FinishGames::Outcome outcome = run({"finish-games", "1002"});
        QVERIFY(!outcome.refusal);
        QCOMPARE(outcome.notes, QStringList({"at ada's logout: child ada added 1 removed 1"}));
        QCOMPARE(lines("helper-calls"), QStringList({"--copy-games ada 12"}));
        QVERIFY(lines("systemd-run-calls").constFirst().contains("--unit=cairn-copy-games-1002"));
        const QByteArray list = read("games/ada.json");
        QVERIFY(list.contains("\"620\"") && list.contains("\"12\"") && !list.contains("504230"));
        // The copy found Celeste already gone.
        QVERIFY(!read("games-seen-ada/ada.json").contains("504230"));
        QVERIFY(!QFile::exists(path("games-waiting/ada")));
        QCOMPARE(lines("games-finished/ada"),
                 QStringList({"copied ada 12", "child ada added 1 removed 1"}));
        QCOMPARE(read("game-pictures/620.jpg"), QByteArray("a picture"));
        QCOMPARE(QDir(path("game-pictures")).entryList(QDir::Files), QStringList({"620.jpg"}));
        QCOMPARE(read("games.json"), QByteArray(sharedGames));
    }

    // A child who logged in again before the copy keeps waiting for the
    // next logout, and nothing is written down yet.
    void aLogoutWordKeepsTheChildWaiting_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::newRow("logged in") << QByteArray("refused logged-in\n");
        QTest::newRow("still running") << QByteArray("refused still-running\n");
        QTest::newRow("stopped") << QByteArray("refused stopped\n");
    }

    void aLogoutWordKeepsTheChildWaiting() {
        QFETCH(const QByteArray, line);
        give("games-waiting/ada", "ada=620,12\n");
        give("lines-ada", line);
        give("status-ada", "1");
        const FinishGames::Outcome outcome = run({"finish-games", "1002"});
        QVERIFY(!outcome.refusal);
        QCOMPARE(outcome.notes.size(), 1);
        QVERIFY(outcome.notes.constFirst().endsWith(", so it waits for the next logout"));
        QCOMPARE(read("games-waiting/ada"), QByteArray("ada=620,12\n"));
        QVERIFY(QDir(path("games-finished")).isEmpty());
    }

    void aJobThatDoesNotStartKeepsTheChildWaiting() {
        give("games-waiting/ada", "ada=620,12\n");
        give("fail-start", "");
        QVERIFY(run({"finish-games", "1002"}).notes.constFirst().contains("held not-started"));
        QVERIFY(QFile::exists(path("games-waiting/ada")));
    }

    // Any other ending is written down for the window, and the wait ends.
    void anotherEndingIsWrittenDown() {
        give("games-waiting/ada", "ada=620,12\n");
        give("lines-ada", "refused busy\n");
        give("status-ada", "1");
        const FinishGames::Outcome outcome = run({"finish-games", "1002"});
        QCOMPARE(outcome.notes,
                 QStringList({"at ada's logout: child ada added 0 removed 1 held busy"}));
        QVERIFY(!QFile::exists(path("games-waiting/ada")));
        QCOMPARE(lines("games-finished/ada"),
                 QStringList({"child ada added 0 removed 1 held busy"}));
    }

    // A game that left the shared folder since the Save is skipped, and the
    // rest go ahead.
    void aGameNoLongerSharedIsSkipped() {
        give("games-waiting/ada", "ada=620,999,12\n");
        run({"finish-games", "1002"});
        QCOMPARE(lines("helper-calls"), QStringList({"--copy-games ada 12"}));
        QCOMPARE(lines("games-finished/ada"),
                 QStringList({"skipped ada 999 not-shared", "copied ada 12",
                              "child ada added 1 removed 1"}));
    }

    void aFinishedWriteThatFailsKeepsTheChildWaiting() {
        give("games-waiting/ada", "ada=620,12\n");
        QVERIFY(QDir().mkpath(path("games-finished/ada")));
        const FinishGames::Outcome outcome = run({"finish-games", "1002"});
        QVERIFY(!outcome.refusal);
        QVERIFY(outcome.notes.join(' ').contains("could not be written down"));
        QCOMPARE(read("games-waiting/ada"), QByteArray("ada=620,12\n"));
    }

    void aWritableWaitingFileIsRemovedAndNeverCopied() {
        give("games-waiting/ada", "ada=620,12\n");
        QVERIFY(chmod(QFile::encodeName(path("games-waiting/ada")).constData(), 0666) == 0);
        const FinishGames::Outcome outcome = run({"finish-games", "1002"});
        QVERIFY(outcome.notes.join(' ').contains("failed a check"));
        QVERIFY(!QFile::exists(path("games-waiting/ada")));
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
    }

    // A waiting file that fails a check is removed and never copied.
    void aFileThatFailsACheckIsRemoved() {
        give("games-waiting/ada", "ben=620,12\n");
        const FinishGames::Outcome outcome = run({"finish-games", "1002"});
        QVERIFY(!outcome.refusal);
        QCOMPARE(outcome.notes.size(), 1);
        QVERIFY(outcome.notes.constFirst().contains("failed a check"));
        QVERIFY(!QFile::exists(path("games-waiting/ada")));
        nothingRan();
    }

    // A Save running at the logout ends first. One that settled the child
    // meanwhile leaves nothing to do.
    void waitsForASaveThenReadsAgain() {
        give("games-waiting/ada", "ada=620,12\n");
        QSemaphore held;
        const std::unique_ptr<QThread> save(QThread::create([this, &held] {
            QLockFile lock(path("lock"));
            lock.setStaleLockTime(0);
            if (lock.tryLock(0)) {
                held.release();
                QThread::msleep(300);
                QFile::remove(path("games-waiting/ada"));
            }
        }));
        save->start();
        QVERIFY(held.tryAcquire(1, 5000));
        QElapsedTimer timer;
        timer.start();
        const FinishGames::Outcome outcome = run({"finish-games", "1002"});
        QVERIFY(timer.elapsed() >= 250);
        QVERIFY(save->wait(5000));
        QVERIFY(!outcome.refusal);
        QVERIFY(outcome.notes.isEmpty());
        nothingRan();
    }
};

} // namespace

QTEST_GUILESS_MAIN(FinishGamesTest)
#include "tst_finishgames.moc"
