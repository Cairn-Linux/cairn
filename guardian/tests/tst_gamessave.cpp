// SPDX-License-Identifier: Apache-2.0
#include "GamesSave.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QLockFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace {

// Portal 2, Zoo Tycoon and Celeste are in the shared folder.
constexpr auto sharedGames = R"({"version": 1, "entries": [
  {"id": "steam-620", "title": "Portal 2", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "620"]},
  {"id": "steam-504230", "title": "Celeste", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "504230"]},
  {"id": "steam-12", "title": "Zoo Tycoon", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "12"]}
]}
)";

// ada already has Portal 2.
constexpr auto adaHasPortal = R"({"version": 1, "entries": [
  {"id": "steam-620", "title": "Portal 2", "category": "games", "engine": "steam",
   "exec": ["steam", "-applaunch", "620"]}
]}
)";

// Only ada, ben and cy are children's names.
QHash<QString, Account> family() {
    return {
        {"ada", {.uid = 1002, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = {}}},
        {"ben", {.uid = 1003, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = {}}},
        {"gina",
         {.uid = 1001,
          .groups = {"gina", "wheel", "cairn-guardian"},
          .login = "gina",
          .fullName = {}}},
        {"daemon", {.uid = 2, .groups = {"daemon", "cairn-l1"}, .login = "daemon", .fullName = {}}},
        {"admin-child",
         {.uid = 1007,
          .groups = {"admin-child", "wheel", "cairn-l1"},
          .login = "admin-child",
          .fullName = {}}},
        {"guardian-child",
         {.uid = 1008,
          .groups = {"guardian-child", "cairn-guardian", "cairn-l2"},
          .login = "guardian-child",
          .fullName = {}}},
    };
}

class GamesSaveTest : public QObject {
    Q_OBJECT

    GamesSave::Outcome run(const QStringList& arguments) {
        GamesSave::Setup setup;
        setup.euid = m_euid;
        setup.pkexecUid = m_pkexecUid;
        const QHash<QString, Account> accounts = family();
        setup.lookUp = [accounts](const QString& name) -> std::optional<Account> {
            const auto found = accounts.constFind(name);
            return found == accounts.constEnd() ? std::nullopt : std::optional(*found);
        };
        setup.lookUpUid = [accounts](unsigned int uid) -> std::optional<Account> {
            for (const Account& account : accounts) {
                if (account.uid == uid) {
                    return account;
                }
            }
            return std::nullopt;
        };
        setup.lookUpHome = [](unsigned int uid) -> std::optional<QString> {
            return QStringLiteral("/var/home/user%1").arg(uid);
        };
        setup.catalogue.python = path("python");
        setup.catalogue.pictureStep = QStringLiteral("/x/cairn-game-pictures");
        setup.catalogue.kidscan = QStringLiteral("/x/kidscan.py");
        setup.catalogue.library = path("library");
        setup.catalogue.pictures = path("pictures");
        setup.catalogue.gamesFile = path("games.json");
        setup.catalogue.restorecon = path("restorecon");
        // The tools step's stand-ins have a folder of their own, so a test
        // tells its calls from the copies'.
        setup.tools.systemdRun = path("tools/systemd-run");
        setup.tools.python = path("tools/python");
        setup.tools.shareStep = QStringLiteral("/x/cairn-share-tools");
        setup.copy.systemdRun = path("systemd-run");
        setup.copy.systemdInhibit = path("systemd-inhibit");
        setup.copy.visitHelper = path("cairn-steam-visit");
        setup.gamesFolder = path("games");
        setup.gamesFolderOwner = getuid();
        setup.lock = path("lock");
        return GamesSave(setup).run(arguments,
                                    [this](const QString& line) { m_said.append(line); });
    }

    void nothingChanged() const {
        QVERIFY(!QFile::exists(path("python-calls")));
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
        QVERIFY(!QFile::exists(path("tools/python-calls")));
        QVERIFY(!QFile::exists(path("tools/systemd-run-calls")));
        QVERIFY(!QFile::exists(path("games.json")));
        QVERIFY(QDir(path("games")).isEmpty());
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

    std::unique_ptr<QTemporaryDir> m_temporary;
    QStringList m_said;
    unsigned int m_euid = 0;
    std::optional<QString> m_pkexecUid;

private slots:
    void init() {
        m_temporary = std::make_unique<QTemporaryDir>();
        QVERIFY(m_temporary->isValid());
        const QHash<QString, QString> links{{"python", "python"},
                                            {"restorecon", "system-tool"},
                                            {"systemd-run", "systemd-run"},
                                            {"systemd-inhibit", "systemd-inhibit"},
                                            {"cairn-steam-visit", "copy-helper"}};
        for (auto link = links.cbegin(); link != links.cend(); ++link) {
            QVERIFY(QFile::link(QStringLiteral(FIXTURES "/") + link.value(), path(link.key())));
        }
        QVERIFY(QDir().mkpath(path("tools")));
        for (const char* name : {"systemd-run", "python"}) {
            QVERIFY(QFile::link(QStringLiteral(FIXTURES "/") + name, path("tools/") + name));
        }
        give("kidscan-output", sharedGames);
        QVERIFY(QDir().mkpath(path("games")));
        m_said.clear();
        m_euid = 0;
        m_pkexecUid = QStringLiteral("1001");
    }

    // Two children, each in turn: the pictures, the list and the tools
    // folder first, then each child's copy and file.
    void savesTwoChildren() {
        const GamesSave::Outcome outcome = run({"save-games", "ada=620,12", "ben=504230"});
        QVERIFY(!outcome.refusal.has_value());
        QCOMPARE(m_said, QStringList({"looking", "pictures made", "catalogue made", "tools made",
                                      "copying ada", "copied ada 620", "copied ada 12",
                                      "child ada added 2 removed 0", "copying ben",
                                      "copied ben 504230", "child ben added 1 removed 0"}));
        QCOMPARE(outcome.asked, QStringLiteral("save-games ada ben"));
        QCOMPARE(outcome.children,
                 QStringList({"child ada added 2 removed 0", "child ben added 1 removed 0"}));
        QCOMPARE(lines("helper-calls"),
                 QStringList({"--copy-games ada 620 12", "--copy-games ben 504230"}));
        QVERIFY(lines("python-calls").constFirst().contains("--home /var/home/user1001 "));
        const QByteArray ada = read("games/ada.json");
        QVERIFY(ada.contains("\"620\"") && ada.contains("\"12\"") && !ada.contains("504230"));
        QVERIFY(read("games/ben.json").contains("\"504230\""));
        QCOMPARE(read("games.json"), QByteArray(sharedGames));
    }

    // With no child named, only the pictures, the full list and the tools
    // folder are made again, and no child's file is touched, even one that
    // cannot be read.
    void aLookChangesNoList() {
        give("games/ada.json", "not a list");
        const GamesSave::Outcome outcome = run({"save-games"});
        QVERIFY(!outcome.refusal.has_value());
        QCOMPARE(m_said, QStringList({"looking", "pictures made", "catalogue made", "tools made"}));
        QCOMPARE(lines("tools/python-calls"), QStringList({"-I -B /x/cairn-share-tools"}));
        QCOMPARE(outcome.asked, QStringLiteral("save-games"));
        QVERIFY(outcome.children.isEmpty());
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
        QCOMPARE(read("games/ada.json"), QByteArray("not a list"));
        QCOMPARE(QDir(path("games")).entryList(QDir::Files), QStringList({"ada.json"}));
        QCOMPARE(read("games.json"), QByteArray(sharedGames));
    }

    void refusesBeforeAnythingChanges_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QString>("word");
        QTest::newRow("unknown") << QStringList{"save-games", "cy=620"} << "not-a-child";
        QTest::newRow("a grown-up") << QStringList{"save-games", "gina=620"} << "not-a-child";
        QTest::newRow("a system account")
            << QStringList{"save-games", "daemon=620"} << "not-a-child";
        QTest::newRow("wheel and a level")
            << QStringList{"save-games", "admin-child=620"} << "not-a-child";
        QTest::newRow("guardian and a level")
            << QStringList{"save-games", "guardian-child="} << "not-a-child";
        QTest::newRow("one child of two")
            << QStringList{"save-games", "ada=620", "gina=620"} << "not-a-child";
        QTest::newRow("usage") << QStringList{"save-games", "ada"} << "usage";
        QTest::newRow("a bad name") << QStringList{"save-games", "Ada=620"} << "bad-name";
        QTest::newRow("a bad appid")
            << QStringList{"save-games", "ada=620", "ben=--all"} << "bad-appid";
    }

    // No picture step, no copy, no file.
    void refusesBeforeAnythingChanges() {
        QFETCH(const QStringList, arguments);
        QFETCH(const QString, word);
        const GamesSave::Outcome outcome = run(arguments);
        QCOMPARE(refusalWord(outcome.refusal.value_or(Refusal::Failed)), word);
        QVERIFY(m_said.isEmpty());
        QVERIFY(outcome.children.isEmpty());
        nothingChanged();
    }

    void refusesAnyoneButAGrownUp() {
        m_pkexecUid = QStringLiteral("1002");
        QCOMPARE(run({"save-games", "ada=620"}).refusal, std::optional(Refusal::NotAGrownUp));
        nothingChanged();
        m_euid = 1001;
        QCOMPARE(run({"save-games", "ada=620"}).refusal, std::optional(Refusal::NotRoot));
        nothingChanged();
    }

    // A second Save waits for nothing and changes nothing.
    void refusesWhileAnotherSaveRuns() {
        QLockFile other(path("lock"));
        QVERIFY(other.tryLock(0));
        const GamesSave::Outcome outcome = run({"save-games", "ada=620"});
        QCOMPARE(outcome.refusal, std::optional(Refusal::Busy));
        QCOMPARE(outcome.asked, QStringLiteral("save-games ada"));
        nothingChanged();
    }

    // Under sudo the pictures and the tools folder stay; the rest goes
    // ahead, and the copy still takes the tools the folder has.
    void sudoKeepsThePicturesAndTheTools() {
        m_pkexecUid = std::nullopt;
        const GamesSave::Outcome outcome = run({"save-games", "ada=620"});
        QCOMPARE(m_said.mid(0, 5), QStringList({"looking", "pictures kept", "catalogue made",
                                                "tools kept", "copying ada"}));
        QCOMPARE(lines("python-calls").size(), 1);
        QVERIFY(!QFile::exists(path("tools/systemd-run-calls")));
        QVERIFY(!QFile::exists(path("tools/python-calls")));
        QCOMPARE(outcome.tools, QStringList({"tools kept"}));
    }

    // The tools step runs as the Guardian pkexec names, after every child's
    // list has lost the games taken off and before the first copy, which
    // takes the tools from the folder it fills (ADR-0046).
    void theToolsComeAfterTheListsAndBeforeTheCopies() {
        give("games/ada.json", adaHasPortal);
        give("tools/see", path("games").toUtf8());
        const GamesSave::Outcome outcome = run({"save-games", "ada=12"});
        QVERIFY(!outcome.refusal.has_value());
        QCOMPARE(m_said.mid(2, 3), QStringList({"catalogue made", "tools made", "copying ada"}));
        QVERIFY(lines("tools/systemd-run-calls").constFirst().contains(" --uid=gina "));
        QVERIFY(lines("tools/systemd-run-calls")
                    .constFirst()
                    .endsWith(" -- " + path("tools/python") + " -I -B /x/cairn-share-tools"));
        // Portal 2 was taken off before the tools step looked.
        QVERIFY(QFile::exists(path("tools/seen/ada.json")));
        QVERIFY(!read("tools/seen/ada.json").contains("\"620\""));
        QCOMPARE(outcome.tools, QStringList({"tools made"}));
    }

    // The tools left out are said, and every line worth one goes to the
    // journal with the Save's.
    void toolsLeftOutAreSaid() {
        give("tools/share-tools-output", "tool-shared 1070560\n"
                                         "tool-had 1391110\n"
                                         "tool-not-shared 4183110 other-drive\n"
                                         "finished\n");
        const GamesSave::Outcome outcome = run({"save-games", "ada=620"});
        QCOMPARE(m_said.mid(3, 3),
                 QStringList({"tools made", "tool-not-shared 4183110 other-drive", "copying ada"}));
        QCOMPARE(outcome.tools, QStringList({"tools made", "tool-shared 1070560",
                                             "tool-not-shared 4183110 other-drive"}));
    }

    // A tools step that fails, or is stopped, keeps the folder as it was,
    // and the Save goes on.
    void aToolsStepThatFailsKeepsTheFolder_data() {
        QTest::addColumn<QString>("given");
        QTest::addColumn<QByteArray>("output");
        QTest::newRow("failed") << "fail-share-tools" << QByteArray();
        QTest::newRow("stopped mid-way") << "" << QByteArray("tool-shared 1070560\n");
        QTest::newRow("not started") << "fail-start" << QByteArray();
        QTest::newRow("refused folder") << "" << QByteArray("refused folder\n");
    }

    void aToolsStepThatFailsKeepsTheFolder() {
        QFETCH(const QString, given);
        QFETCH(const QByteArray, output);
        if (!given.isEmpty()) {
            give("tools/" + given, "");
        }
        if (!output.isEmpty()) {
            give("tools/share-tools-output", output);
        }
        const GamesSave::Outcome outcome = run({"save-games", "ada=620"});
        QVERIFY(!outcome.refusal.has_value());
        QCOMPARE(m_said.mid(3), QStringList({"tools kept", "copying ada", "copied ada 620",
                                             "child ada added 1 removed 0"}));
        QCOMPARE(outcome.tools.constFirst(), QStringLiteral("tools kept"));
    }

    // A logged-in child loses the game taken off at once and gets the new
    // one later; the next child still goes ahead.
    void aLoggedInChildIsHeld() {
        give("games/ada.json", adaHasPortal);
        give("lines-ada", "refused logged-in\n");
        give("status-ada", "1");
        const GamesSave::Outcome outcome = run({"save-games", "ada=12", "ben=620"});
        QVERIFY(!outcome.refusal.has_value());
        QCOMPARE(m_said.mid(4),
                 QStringList({"copying ada", "child ada added 0 removed 1 held logged-in",
                              "copying ben", "copied ben 620", "child ben added 1 removed 0"}));
        QVERIFY(!read("games/ada.json").contains("\"620\""));
        QVERIFY(!read("games/ada.json").contains("\"12\""));
    }

    // A game no longer shared is never passed to the copy.
    void aGameNotSharedIsSkipped() {
        run({"save-games", "ada=999,620"});
        QCOMPARE(m_said.mid(4, 2), QStringList({"skipped ada 999 not-shared", "copying ada"}));
        QCOMPARE(lines("helper-calls"), QStringList({"--copy-games ada 620"}));
        QVERIFY(!read("games/ada.json").contains("999"));
    }

    // Games already on the list are not copied again; with nothing new,
    // nothing is copied at all.
    void listedGamesAreNotCopiedAgain() {
        give("games/ada.json", adaHasPortal);
        run({"save-games", "ada=620,12"});
        QCOMPARE(lines("helper-calls"), QStringList({"--copy-games ada 12"}));
        QVERIFY(m_said.contains("child ada added 1 removed 0"));

        QFile::remove(path("helper-calls"));
        QFile::remove(path("systemd-run-calls"));
        m_said.clear();
        run({"save-games", "ada=620,12"});
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
        QCOMPARE(m_said.mid(4), QStringList({"child ada added 0 removed 0"}));
    }

    // Every child's games taken off are gone before the first copy starts,
    // so a Save cut off mid-copy leaves no child a game taken off.
    void gamesTakenOffGoBeforeAnyCopy() {
        give("games/ada.json", adaHasPortal);
        give("games/ben.json", adaHasPortal);
        run({"save-games", "ada=12", "ben=504230"});
        QVERIFY(!read("games-seen-ada/ada.json").contains("\"620\""));
        QVERIFY(!read("games-seen-ada/ben.json").contains("\"620\""));
        QVERIFY(!read("games-seen-ada/ada.json").contains("\"12\""));
        QVERIFY(m_said.contains("child ada added 1 removed 1"));
        QVERIFY(m_said.contains("child ben added 1 removed 1"));
    }

    // A list that cannot be written after the copy holds what was written
    // before it, and the counts say so.
    void aWriteAfterTheCopyFailsWithTheGamesTakenOffGone() {
        give("games/ada.json", adaHasPortal);
        give("break-ada", "");
        run({"save-games", "ada=12"});
        QVERIFY(m_said.contains("child ada added 0 removed 1 held not-written"));
    }

    // Without a full games list no new game can be given, and the reason
    // says the list is missing, not that the game is not shared.
    void noCatalogueIsNotNotShared() {
        give("fail-kidscan", "");
        give("games/ada.json", adaHasPortal);
        run({"save-games", "ada=620,12"});
        QCOMPARE(m_said.mid(2),
                 QStringList({"catalogue kept", "tools made", "skipped ada 12 no-catalogue",
                              "child ada added 0 removed 0"}));
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
        QVERIFY(read("games/ada.json").contains("\"620\""));
    }

    // One child's file that cannot be written does not stop the others.
    void aFailedWriteHoldsOnlyThatChild() {
        QVERIFY(QDir().mkpath(path("games/ada.json")));
        const GamesSave::Outcome outcome = run({"save-games", "ada=620", "ben=620"});
        QVERIFY(!outcome.refusal.has_value());
        QVERIFY(m_said.contains("child ada added 0 removed 0 held not-written"));
        QVERIFY(m_said.contains("child ben added 1 removed 0"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(GamesSaveTest)
#include "tst_gamessave.moc"
