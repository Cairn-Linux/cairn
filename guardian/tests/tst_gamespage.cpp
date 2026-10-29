// SPDX-License-Identifier: Apache-2.0
#include "GamesPage.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <unistd.h>

namespace {

constexpr int kAnswerMilliseconds = 10000;

QJsonObject game(const QString& appId, const QString& title) {
    return {{"id", "steam-" + appId},
            {"title", title},
            {"category", "games"},
            {"exec", QJsonArray{"steam", "-applaunch", appId}}};
}

QByteArray gamesFile(const QJsonArray& entries) {
    return QJsonDocument(QJsonObject{{"version", 1}, {"entries", entries}}).toJson();
}

// A Guardian and three children, out of order as NSS may list them, and an
// account in no level group.
std::vector<Account> family() {
    return {
        {.uid = 1002, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = "Ben"},
        {.uid = 1000,
         .groups = {"guardian", "wheel", "cairn-guardian"},
         .login = "guardian",
         .fullName = "Guardian"},
        {.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"},
        {.uid = 1003, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = {}},
        {.uid = 1004, .groups = {"guest"}, .login = "guest", .fullName = {}},
    };
}

class GamesPageTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    static void write(const QString& path, const QByteArray& bytes) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    // A page over the files in the temporary folder, whose Steam window
    // start is written down and answers as `starts` says.
    std::unique_ptr<GamesPage> makePage(bool starts = true) {
        GamesPage::Setup setup;
        setup.pkexec = FIXTURES "/pkexec-family";
        setup.command = FIXTURES "/cairn-family";
        setup.catalogue = path("games.json");
        setup.lists = {.childLists = path("games"),
                       .waiting = path("games-waiting"),
                       .finished = path("games-finished"),
                       .owner = getuid()};
        setup.steamFor = "/usr/bin/cairn-steam-for";
        setup.accounts = [this] { return m_accounts; };
        setup.start = [this, starts](const QString& program, const QStringList& arguments) {
            m_started.append(program + " " + arguments.join(' '));
            return starts;
        };
        setup.loggedIn = [this] {
            ++m_logindAsked;
            return m_loggedIn;
        };
        return std::make_unique<GamesPage>(setup);
    }

    static QStringList listed(const GamesPage& page) {
        QStringList titles;
        for (const QVariant& value : page.games()) {
            const QVariantMap game = value.toMap();
            if (page.wanted().contains(game.value("appId").toString())) {
                titles.append(game.value("title").toString());
            }
        }
        return titles;
    }

    // The games the page shows for the child picked, each as appId=state:
    // on, off, added or taken.
    static QStringList states(const GamesPage& page) {
        QStringList states;
        for (const QVariant& value : page.games()) {
            const QVariantMap game = value.toMap();
            const bool onList = page.wanted().contains(game.value("appId").toString());
            const bool saved = game.value("saved").toBool();
            QString state = onList ? QStringLiteral("added") : QStringLiteral("taken");
            if (onList == saved) {
                state = onList ? QStringLiteral("on") : QStringLiteral("off");
            }
            states.append(game.value("appId").toString() + "=" + state);
        }
        return states;
    }

    // Runs a Save, or a look for new games, with the stand-ins answering as
    // the scenario says, and returns the arguments cairn-family was given,
    // each run starting with a line "run". `answer` answers the question
    // about a logged-in child, when one opens.
    QString saveWith(GamesPage& page, const QString& scenario, bool look = false,
                     const std::function<void(LogOutQuestion&)>& answer = {}) {
        QFile::remove(path("arguments"));
        qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
        qputenv("CAIRN_TEST_ARGUMENTS", path("arguments").toLocal8Bit());
        qputenv("CAIRN_TEST_GAMES", path("games").toLocal8Bit());
        qputenv("CAIRN_TEST_WAITING", path("games-waiting").toLocal8Bit());
        QSignalSpy ended(page.saving(), &GamesSaving::ended);
        look ? page.lookForGames() : page.save();
        if (answer && page.question()->asking()) {
            answer(*page.question());
        }
        const bool answered = !ended.isEmpty() || ended.wait(kAnswerMilliseconds);
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        qunsetenv("CAIRN_TEST_GAMES");
        qunsetenv("CAIRN_TEST_WAITING");
        if (!answered) {
            return QStringLiteral("no answer");
        }
        QFile file(path("arguments"));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }

    QTemporaryDir m_temporary;
    std::vector<Account> m_accounts;
    QStringList m_started;
    QSet<QString> m_loggedIn;
    int m_logindAsked = 0;

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        m_accounts = family();
        m_started.clear();
        m_loggedIn.clear();
        m_logindAsked = 0;
        write(path("games.json"), gamesFile({game("1001", "Game one"), game("1002", "Game two"),
                                             game("1003", "Game three")}));
        for (const char* folder : {"games", "games-waiting", "games-finished"}) {
            QVERIFY(QDir().mkpath(path(folder)));
        }
        write(path("games/ada.json"), gamesFile({game("1002", "Game two")}));
    }

    // Whether the card for a game says it waits for the child's logout.
    static bool waits(const GamesPage& page, const QString& appId) {
        for (const QVariant& value : page.games()) {
            const QVariantMap game = value.toMap();
            if (game.value("appId").toString() == appId) {
                return game.value("waiting").toBool();
            }
        }
        return false;
    }

    // How the copy at a child's last logout ended, dated.
    void finish(const QString& login, const QByteArray& lines, const QDateTime& when) {
        write(path("games-finished/" + login), lines);
        QFile file(path("games-finished/" + login));
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(when, QFileDevice::FileModificationTime));
    }

    void cleanup() {
        QDir(m_temporary.path()).removeRecursively();
        QDir().mkpath(m_temporary.path());
    }

    // The children as on the login screen, the first one picked.
    void itListsTheChildrenAndPicksTheFirst() {
        const auto page = makePage();
        QStringList names;
        for (const QVariant& child : page->listedChildren()) {
            names.append(child.toMap().value("login").toString() + "=" +
                         child.toMap().value("name").toString());
        }
        QCOMPARE(names, QStringList({"ada=Ada", "ben=Ben", "cy=cy"}));
        QCOMPARE(page->chosen(), "ada");
        QCOMPARE(page->chosenName(), "Ada");
        QCOMPARE(page->games().size(), 3);
        QCOMPARE(listed(*page), QStringList({"Game two"}));
        QCOMPARE(page->onListCount(), 1);
        QCOMPARE(page->list(), GamesPage::List::Read);
    }

    void pickingAChildShowsTheirList() {
        const auto page = makePage();
        const QSignalSpy changed(page.get(), &GamesPage::changed);
        page->choose("ben");
        QCOMPARE(changed.count(), 1);
        QCOMPARE(page->chosen(), "ben");
        QCOMPARE(page->chosenName(), "Ben");
        QCOMPARE(listed(*page), QStringList());
        QCOMPARE(page->list(), GamesPage::List::None);
        QCOMPARE(page->games().size(), 3);
    }

    // Only a listed child may be picked.
    void anotherAccountIsNeverPicked() {
        const auto page = makePage();
        page->choose("guardian");
        page->choose("guest");
        page->choose("../ada");
        QCOMPARE(page->chosen(), "ada");
    }

    void aListThatCannotBeReadSaysSo() {
        write(path("games/cy.json"), "{");
        const auto page = makePage();
        page->choose("cy");
        QCOMPARE(page->list(), GamesPage::List::Unreadable);
        QCOMPARE(page->onListCount(), 0);
    }

    // The login goes into a file name only when it follows cairn-family's
    // rule; otherwise the child reads as having no list.
    void aLoginOutsideTheRuleReadsNoFile() {
        m_accounts.push_back(
            {.uid = 1005, .groups = {"Dee", "cairn-l1"}, .login = "Dee", .fullName = {}});
        write(path("games/Dee.json"), gamesFile({game("1001", "Game one")}));
        const auto page = makePage();
        page->choose("Dee");
        QCOMPARE(page->chosen(), "Dee");
        QCOMPARE(page->list(), GamesPage::List::None);
        QCOMPARE(listed(*page), QStringList());
    }

    void eachGameCarriesWhatTheCardNeeds() {
        const auto page = makePage();
        const QVariantMap second = page->games().at(1).toMap();
        QCOMPARE(second.value("appId").toString(), "1002");
        QCOMPARE(second.value("title").toString(), "Game two");
        QVERIFY(second.value("picture").toUrl().isEmpty());
        QCOMPARE(second.value("saved").toBool(), true);
        QVERIFY(page->wanted().contains("1002"));
    }

    void noChildrenPicksNoOne() {
        m_accounts = {family().at(1)};
        const auto page = makePage();
        QVERIFY(page->listedChildren().isEmpty());
        QCOMPARE(page->chosen(), QString());
        QCOMPARE(page->chosenName(), QString());
        QCOMPARE(page->games().size(), 3);
        QCOMPARE(page->onListCount(), 0);
    }

    // No shared games: only the games left on the child's own list show.
    void noSharedGames() {
        QVERIFY(QFile::remove(path("games.json")));
        const auto page = makePage();
        QCOMPARE(listed(*page), QStringList({"Game two"}));
        QCOMPARE(page->sharedCount(), 0);
        QCOMPARE(page->goneCount(), 1);
        page->choose("ben");
        QVERIFY(page->games().isEmpty());
        QCOMPARE(page->chosen(), "ben");
    }

    // Reading again finds a new child and a changed list, and the child
    // picked stays picked while they are there.
    void reloadReadsEverythingAgain() {
        const auto page = makePage();
        page->choose("ben");
        m_accounts.push_back(
            {.uid = 1006, .groups = {"dot", "cairn-l2"}, .login = "dot", .fullName = "Dot"});
        write(path("games/ben.json"), gamesFile({game("1001", "Game one")}));
        page->reload();
        QCOMPARE(page->listedChildren().size(), 4);
        QCOMPARE(page->chosen(), "ben");
        QCOMPARE(listed(*page), QStringList({"Game one"}));
    }

    // The same children keep the same list, so the page's controls and
    // keyboard focus stay; a new child changes it.
    void theChildrenChangeOnlyWhenTheyDo() {
        const auto page = makePage();
        const QSignalSpy children(page.get(), &GamesPage::listedChildrenChanged);
        page->reload();
        page->choose("ben");
        QCOMPARE(children.count(), 0);
        m_accounts.push_back(
            {.uid = 1006, .groups = {"dot", "cairn-l2"}, .login = "dot", .fullName = "Dot"});
        page->reload();
        QCOMPARE(children.count(), 1);
    }

    void reloadPicksTheFirstWhenThePickedChildIsGone() {
        const auto page = makePage();
        page->choose("ben");
        std::erase_if(m_accounts, [](const Account& account) { return account.login == "ben"; });
        page->reload();
        QCOMPARE(page->chosen(), "ada");
    }

    // The Steam window, as the menu entry starts it, on its own.
    void openSteamStartsTheSteamWindow() {
        const auto page = makePage();
        page->openSteam("ben");
        QCOMPARE(m_started, QStringList({"/usr/bin/cairn-steam-for ben"}));
        QCOMPARE(page->steamNotOpened(), QString());
    }

    void aSteamWindowThatDoesNotStartSaysWhose() {
        const auto page = makePage(false);
        // The page sends the parent to this line in the system log.
        QTest::ignoreMessage(QtWarningMsg, "cairn-guardian: /usr/bin/cairn-steam-for could not be "
                                           "started for ben, so its Steam window did not open.");
        page->openSteam("ben");
        QCOMPARE(page->steamNotOpened(), "Ben");
        // Picking another child clears it.
        page->choose("cy");
        QCOMPARE(page->steamNotOpened(), QString());
    }

    // A game on the child's list that the shared folder no longer has is
    // counted apart from the shared ones.
    void aGameNoLongerSharedIsCountedApart() {
        write(path("games/ada.json"), gamesFile({game("1002", "Game two"), game("2001", "Gone")}));
        const auto page = makePage();
        QCOMPARE(listed(*page), QStringList({"Game two", "Gone"}));
        QCOMPARE(page->onListCount(), 2);
        QCOMPARE(page->sharedCount(), 3);
        QCOMPARE(page->goneCount(), 1);
        QCOMPARE(page->games().last().toMap().value("shared").toBool(), false);
        QCOMPARE(page->games().first().toMap().value("shared").toBool(), true);
    }

    void openSteamOnlyForAChild() {
        const auto page = makePage();
        page->openSteam("guardian");
        page->openSteam("guest");
        page->openSteam("");
        QVERIFY(m_started.isEmpty());
    }

    // ---- choosing games, and Save ----

    void turningAGameMarksItUntilSave() {
        const auto page = makePage();
        QVERIFY(!page->canSave());
        page->turn("1001");
        page->turn("1002");
        QCOMPARE(states(*page), QStringList({"1001=added", "1002=taken", "1003=off"}));
        QCOMPARE(listed(*page), QStringList({"Game one"}));
        QCOMPARE(page->onListCount(), 1);
        QCOMPARE(page->unsavedLogins(), QStringList({"ada"}));
        QCOMPARE(page->unsavedNames(), QStringLiteral("Ada"));
        QVERIFY(page->canSave());
        // Turning both back leaves nothing to save.
        page->turn("1001");
        page->turn("1002");
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=on", "1003=off"}));
        QVERIFY(!page->canSave());
    }

    // Turning a game changes only the list as wanted, never the games, so
    // the page keeps its cards and keyboard focus.
    void turningKeepsTheGames() {
        const auto page = makePage();
        const QSignalSpy games(page.get(), &GamesPage::gamesChanged);
        const QSignalSpy changed(page.get(), &GamesPage::changed);
        page->turn("1001");
        page->reload();
        QCOMPARE(games.count(), 0);
        QCOMPARE(changed.count(), 2);
        page->choose("ben");
        QCOMPARE(games.count(), 1);
    }

    // Picking another child keeps the first child's changes.
    void switchingChildKeepsTheChanges() {
        const auto page = makePage();
        page->turn("1001");
        page->choose("ben");
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=off", "1003=off"}));
        page->turn("1003");
        page->choose("ada");
        QCOMPARE(states(*page), QStringList({"1001=added", "1002=on", "1003=off"}));
        QCOMPARE(page->unsavedLogins(), QStringList({"ada", "ben"}));
        QCOMPARE(page->unsavedNames(), QStringLiteral("Ada and Ben"));
        // Showing the page again reads the files again and keeps them too.
        page->reload();
        QCOMPARE(page->unsavedLogins(), QStringList({"ada", "ben"}));
    }

    void putBackUndoesOnlyThePickedChild() {
        const auto page = makePage();
        page->turn("1001");
        page->choose("ben");
        QVERIFY(!page->canPutBack());
        page->turn("1003");
        QVERIFY(page->canPutBack());
        page->putBack();
        QVERIFY(!page->canPutBack());
        QCOMPARE(page->unsavedLogins(), QStringList({"ada"}));
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=off", "1003=off"}));
    }

    // Only a game the page shows can be turned.
    void aGameNotShownCannotBeTurned() {
        const auto page = makePage();
        page->turn("4242");
        page->turn("");
        QVERIFY(!page->canSave());
    }

    // A game on the list that the shared folder no longer has can be taken
    // off, and stays shown, taken off, until Save.
    void aGameNoLongerSharedCanBeTakenOff() {
        write(path("games/ada.json"), gamesFile({game("1002", "Game two"), game("2001", "Gone")}));
        const auto page = makePage();
        page->turn("2001");
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=on", "1003=off", "2001=taken"}));
        QCOMPARE(saveWith(*page, "done"), QStringLiteral("run\nsave-games\nada=1002\n"));
    }

    // One run for every changed child, each with their whole new list, in
    // the children's order; an empty list is nothing after the =.
    void saveGivesEachChangedChildTheirWholeList() {
        const auto page = makePage();
        page->choose("cy");
        page->turn("1003");
        page->choose("ada");
        page->turn("1001");
        page->turn("1002");
        page->choose("ben");
        QCOMPARE(saveWith(*page, "done"), QStringLiteral("run\nsave-games\nada=1001\ncy=1003\n"));
        // The stand-in wrote no list, so both changes are still there.
        page->choose("ada");
        page->turn("1001");
        QCOMPARE(saveWith(*page, "done"), QStringLiteral("run\nsave-games\nada=\ncy=1003\n"));
    }

    // After a Save the lists are read again, so the page shows what is
    // saved and there is nothing left to save.
    void aSaveThatGaveEverything() {
        const auto page = makePage();
        page->turn("1001");
        page->choose("ben");
        page->turn("1003");
        saveWith(*page, "save-done");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        QVERIFY(!page->canSave());
        QVERIFY(page->unsavedLogins().isEmpty());
        QCOMPARE(page->chosen(), "ben");
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=off", "1003=on"}));
        const QVariantList results = page->saving()->results();
        QCOMPARE(results.size(), 2);
        const QVariantMap ada = results.at(0).toMap();
        QCOMPARE(ada.value("name").toString(), "Ada");
        QCOMPARE(ada.value("added").toInt(), 2);
        QCOMPARE(ada.value("removed").toInt(), 0);
        QCOMPARE(ada.value("held").toInt(), static_cast<int>(GamesSaving::Held::NotHeld));
        QCOMPARE(ada.value("later").toBool(), false);
        QVERIFY(page->saving()->allGiven());
    }

    // ---- games waiting for a child's next logout ----

    // A child's waiting list is the page's starting point for them: Save
    // stays off until the parent changes something, and the games not
    // copied yet are marked as waiting.
    void aWaitingListIsTheBaseline() {
        write(path("games-waiting/ada"), "ada=1002,1003\n");
        const auto page = makePage();
        QVERIFY(page->chosenWaiting());
        QCOMPARE(page->wanted(), QStringList({"1002", "1003"}));
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=on", "1003=on"}));
        QVERIFY(waits(*page, "1003"));
        QVERIFY(!waits(*page, "1002"));
        QVERIFY(!waits(*page, "1001"));
        QVERIFY(!page->canSave());
        QVERIFY(page->unsavedLogins().isEmpty());
        page->choose("ben");
        QVERIFY(!page->chosenWaiting());
    }

    // Taking a waiting game off and saving sends the new whole list, and
    // Put back as saved returns to the waiting list.
    void aWaitingGameCanBeTakenOff() {
        write(path("games-waiting/ada"), "ada=1002,1003\n");
        const auto page = makePage();
        page->turn("1003");
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=on", "1003=taken"}));
        QVERIFY(page->canSave());
        page->putBack();
        QCOMPARE(page->wanted(), QStringList({"1002", "1003"}));
        QVERIFY(!page->canSave());
        page->turn("1003");
        QCOMPARE(saveWith(*page, "done"), QStringLiteral("run\nsave-games\nada=1002\n"));
    }

    // A waiting file that fails a check is no starting point.
    void aWaitingFileThatFailsACheckIsLeftOut() {
        write(path("games-waiting/ada"), "ben=1003\n");
        const auto page = makePage();
        QVERIFY(!page->chosenWaiting());
        QCOMPARE(page->wanted(), QStringList({"1002"}));
    }

    // An open page follows the files root writes at a logout.
    void anOpenPageFollowsTheFiles() {
        const auto page = makePage();
        QVERIFY(!page->chosenWaiting());
        write(path("games-waiting/ada"), "ada=1002,1003\n");
        QTRY_VERIFY_WITH_TIMEOUT(page->chosenWaiting(), kAnswerMilliseconds);
        QVERIFY(QFile::remove(path("games-waiting/ada")));
        finish("ada", "copied ada 1003\nchild ada added 1 removed 0\n",
               QDateTime::currentDateTime());
        QTRY_VERIFY_WITH_TIMEOUT(!page->finished().isEmpty(), kAnswerMilliseconds);
        QVERIFY(!page->chosenWaiting());
    }

    // A clean finish, dated by the file, until the next Save for the child.
    void aCleanFinishIsDated() {
        const QDateTime when(QDate(2026, 10, 20), QTime(17, 30));
        finish("ada", "copied ada 1003\nchild ada added 1 removed 0\n", when);
        const auto page = makePage();
        const QVariantMap finished = page->finished();
        QCOMPARE(finished.value("when").toDateTime(), when);
        QCOMPARE(finished.value("clean").toBool(), true);
        const QVariantMap result = finished.value("result").toMap();
        QCOMPARE(result.value("name").toString(), "Ada");
        QCOMPARE(result.value("added").toInt(), 1);
        QCOMPARE(result.value("held").toInt(), static_cast<int>(GamesSaving::Held::NotHeld));
        page->choose("ben");
        QVERIFY(page->finished().isEmpty());
    }

    void aFinishThatGaveNotEverything() {
        finish("ada", "skipped ada 1003 downloading\nchild ada added 0 removed 0 held failed\n",
               QDateTime::currentDateTime());
        const auto page = makePage();
        const QVariantMap finished = page->finished();
        QCOMPARE(finished.value("clean").toBool(), false);
        const QVariantMap result = finished.value("result").toMap();
        QCOMPARE(result.value("held").toInt(), static_cast<int>(GamesSaving::Held::CopyStopped));
        const QVariantMap skip = result.value("skipped").toList().constFirst().toMap();
        QCOMPARE(skip.value("title").toString(), "Game three");
    }

    // Only the command's own lines about this child count.
    void aFinishedFileWithoutTheChildsLineShowsNothing() {
        finish("ada", "child ben added 1 removed 0\nit went fine\n", QDateTime::currentDateTime());
        QVERIFY(makePage()->finished().isEmpty());
    }

    // The Save says `later`: the child's games are written down for their
    // next logout, so the page starts from them, and nothing is left to
    // save.
    void aSaveWhoseGamesWaitForALogout() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-held-later");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        QVERIFY(!page->saving()->allGiven());
        const QVariantMap ada = page->saving()->results().constFirst().toMap();
        QCOMPARE(ada.value("held").toInt(), static_cast<int>(GamesSaving::Held::LoggedIn));
        QCOMPARE(ada.value("later").toBool(), true);
        QVERIFY(page->chosenWaiting());
        QVERIFY(!page->canSave());
        QCOMPARE(states(*page), QStringList({"1001=on", "1002=on", "1003=off"}));
        QVERIFY(waits(*page, "1001"));
    }

    // The Save counts the games taken off a child whose games wait, so the
    // page says they are gone only when there were some.
    void aLaterSaveCountsTheGamesTakenOff() {
        const auto page = makePage();
        page->turn("1001");
        page->turn("1002");
        saveWith(*page, "save-held-later");
        const QVariantMap ada = page->saving()->results().constFirst().toMap();
        QCOMPARE(ada.value("later").toBool(), true);
        QCOMPARE(ada.value("removed").toInt(), 1);
    }

    // Once the copy at the child's logout has run, the Save's outcome,
    // which says their games are still to come, ends, and the note about
    // that logout takes its place.
    void aSettledWaitEndsTheOutcome() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-held-later");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        QCOMPARE(page->saving()->laterLogins(), QStringList({"ada"}));
        // Another child's file changing leaves Ada waiting, and the outcome.
        QSignalSpy changed(page.get(), &GamesPage::changed);
        finish("ben", "child ben added 1 removed 0\n", QDateTime::currentDateTime());
        QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty(), kAnswerMilliseconds);
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        QVERIFY(QFile::remove(path("games-waiting/ada")));
        finish("ada", "copied ada 1001\nchild ada added 1 removed 0\n",
               QDateTime::currentDateTime());
        QTRY_COMPARE_WITH_TIMEOUT(page->saving()->step(), GamesSaving::Step::Ready,
                                  kAnswerMilliseconds);
        QVERIFY(page->saving()->laterLogins().isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!page->finished().isEmpty(), kAnswerMilliseconds);
        QVERIFY(!page->chosenWaiting());
    }

    // Two children wait; once Ada's wait has ended, the outcome leaves her
    // out but still speaks for Ben, and ends once Ben's has too.
    void aSettledChildLeavesTheOthersInTheOutcome() {
        const auto page = makePage();
        page->turn("1001");
        page->choose("ben");
        page->turn("1001");
        page->choose("ada");
        saveWith(*page, "save-held-later");
        QCOMPARE(page->saving()->laterLogins(), QStringList({"ada", "ben"}));
        QCOMPARE(page->saving()->results().size(), 2);
        QVERIFY(!page->chosenSettled());
        QVERIFY(QFile::remove(path("games-waiting/ada")));
        finish("ada", "copied ada 1001\nchild ada added 1 removed 0\n",
               QDateTime::currentDateTime());
        QTRY_COMPARE_WITH_TIMEOUT(page->saving()->laterLogins(), QStringList({"ben"}),
                                  kAnswerMilliseconds);
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        const QVariantList results = page->saving()->results();
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.constFirst().toMap().value("login").toString(), "ben");
        QVERIFY(page->chosenSettled());
        QVERIFY(!page->finished().isEmpty());
        QVERIFY(QFile::remove(path("games-waiting/ben")));
        QTRY_COMPARE_WITH_TIMEOUT(page->saving()->step(), GamesSaving::Step::Ready,
                                  kAnswerMilliseconds);
        QVERIFY(!page->chosenSettled());
    }

    // A waiting file that fails the page's checks is not the end of the
    // wait, so the outcome stays and still speaks for the child.
    void aRefusedWaitingFileKeepsTheOutcome() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-held-later");
        QCOMPARE(page->saving()->laterLogins(), QStringList({"ada"}));
        write(path("games-waiting/ada"), "ben=1001\n");
        page->reload();
        QVERIFY(!page->chosenWaiting());
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        QCOMPARE(page->saving()->laterLogins(), QStringList({"ada"}));
        QCOMPARE(page->saving()->results().size(), 1);
        QVERIFY(!page->chosenSettled());
    }

    // A Save that left no child waiting is not ended by a logout.
    void aSaveWithNoWaitIsKept() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-done");
        QSignalSpy changed(page.get(), &GamesPage::changed);
        finish("ada", "child ada added 1 removed 0\n", QDateTime::currentDateTime());
        QTRY_VERIFY_WITH_TIMEOUT(!changed.isEmpty(), kAnswerMilliseconds);
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
    }

    // A child who is logged in gets only the games taken off; with no
    // `later`, their new games stay chosen, so Save can give them later.
    void aLoggedInChildIsHeld() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-held-logged-in");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        const QVariantMap ada = page->saving()->results().constFirst().toMap();
        QCOMPARE(ada.value("held").toInt(), static_cast<int>(GamesSaving::Held::LoggedIn));
        QVERIFY(!page->saving()->allGiven());
        QCOMPARE(page->unsavedLogins(), QStringList({"ada"}));
        QVERIFY(page->canSave());
        QCOMPARE(states(*page), QStringList({"1001=added", "1002=on", "1003=off"}));
    }

    void skippedGamesAreNamed() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-skipped");
        const QVariantList skipped =
            page->saving()->results().constFirst().toMap().value("skipped").toList();
        QCOMPARE(skipped.size(), 2);
        const QVariantMap first = skipped.at(0).toMap();
        QCOMPARE(first.value("appId").toString(), "1002");
        QCOMPARE(first.value("title").toString(), "Game two");
        QCOMPARE(first.value("reason").toInt(), static_cast<int>(GamesSaving::Skip::Downloading));
    }

    // A refusal, or a closed password box, keeps every change to try again.
    void aRefusalKeepsTheChanges_data() {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<GamesSaving::Reason>("reason");
        QTest::newRow("busy") << "refused-busy" << GamesSaving::Reason::AnotherSave;
        QTest::newRow("not a child") << "refused-not-a-child" << GamesSaving::Reason::NotAChild;
        QTest::newRow("not a grown-up")
            << "refused-not-a-grown-up" << GamesSaving::Reason::NotAGrownUp;
        QTest::newRow("bad appid") << "refused-bad-appid" << GamesSaving::Reason::Usage;
        QTest::newRow("dismissed") << "dismissed" << GamesSaving::Reason::Dismissed;
        QTest::newRow("not authorized") << "not-authorized" << GamesSaving::Reason::NotAuthorized;
        QTest::newRow("cut off") << "save-cut-off" << GamesSaving::Reason::Unknown;
        QTest::newRow("crashed") << "crashes" << GamesSaving::Reason::Unknown;
    }

    void aRefusalKeepsTheChanges() {
        QFETCH(const QString, scenario);
        QFETCH(const GamesSaving::Reason, reason);
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, scenario);
        QCOMPARE(page->saving()->step(), GamesSaving::Step::NotSaved);
        QCOMPARE(page->saving()->reason(), reason);
        QCOMPARE(page->unsavedLogins(), QStringList({"ada"}));
        QVERIFY(page->canSave());
    }

    // While the password box is open, nothing on the page changes.
    void everythingWaitsWhileASaveRuns() {
        const auto page = makePage();
        page->turn("1001");
        qputenv("CAIRN_TEST_SCENARIO", "asking");
        page->save();
        QVERIFY(page->busy());
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Asking);
        QVERIFY(!page->canSave());
        page->turn("1003");
        page->putBack();
        page->openSteam("ada");
        QCOMPARE(states(*page), QStringList({"1001=added", "1002=on", "1003=off"}));
        QVERIFY(m_started.isEmpty());
        qputenv("CAIRN_TEST_ARGUMENTS", path("arguments").toLocal8Bit());
        page->save();
        page->lookForGames();
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QVERIFY(!QFile::exists(path("arguments")));
    }

    // Progress comes from the lines as they arrive, with the child's name.
    void aCopyInProgressNamesTheChild() {
        const auto page = makePage();
        page->turn("1001");
        qputenv("CAIRN_TEST_SCENARIO", "save-copying");
        page->save();
        QTRY_COMPARE_WITH_TIMEOUT(page->saving()->progress(), GamesSaving::Progress::CopyingGames,
                                  kAnswerMilliseconds);
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saving);
        QCOMPARE(page->saving()->progressName(), "Ada");
        QVERIFY(page->busy());
    }

    // Looking for new games names no child, so no list the page read, old
    // or unreadable, is written back.
    void lookForGamesChangesNoList() {
        const auto page = makePage();
        page->choose("ben");
        page->turn("1003");
        page->choose("ada");
        QCOMPARE(saveWith(*page, "done", true), QStringLiteral("run\nsave-games\n"));
        QCOMPARE(page->saving()->kind(), GamesSaving::Kind::Look);
        QCOMPARE(page->unsavedLogins(), QStringList({"ben"}));

        write(path("games/ada.json"), "not a list");
        page->reload();
        QCOMPARE(page->list(), GamesPage::List::Unreadable);
        QCOMPARE(saveWith(*page, "done", true), QStringLiteral("run\nsave-games\n"));
    }

    // The first line comes before the pictures are made, so the page stops
    // asking for the password as soon as it is accepted.
    void thePasswordBoxEndsBeforeThePictures() {
        const auto page = makePage();
        qputenv("CAIRN_TEST_SCENARIO", "save-looking");
        page->lookForGames();
        QTRY_COMPARE_WITH_TIMEOUT(page->saving()->step(), GamesSaving::Step::Saving,
                                  kAnswerMilliseconds);
        QCOMPARE(page->saving()->progress(), GamesSaving::Progress::Looking);
    }

    // What the last Save did stops showing once the page changes, so it is
    // never read as being about the lists there now.
    void theOutcomeEndsWithTheNextChange() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-done");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        page->turn("1003");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Ready);
        QVERIFY(page->saving()->results().isEmpty());

        saveWith(*page, "refused-busy");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::NotSaved);
        page->choose("ben");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Ready);
        QCOMPARE(page->saving()->reason(), GamesSaving::Reason::NoReason);

        page->choose("ada");
        saveWith(*page, "dismissed");
        page->putBack();
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Ready);
    }

    // Picking the child already picked changes nothing, so the outcome stays.
    void theOutcomeStaysWithNoChange() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-done");
        page->choose("ada");
        page->choose("nobody");
        page->putBack();
        page->turn("4242");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
    }

    // A login cairn-family would refuse is refused before any password.
    void aLoginOutsideTheRuleIsRefusedWithoutAPassword() {
        m_accounts.push_back(
            {.uid = 1005, .groups = {"Dee", "cairn-l1"}, .login = "Dee", .fullName = {}});
        const auto page = makePage();
        page->choose("Dee");
        page->turn("1001");
        QCOMPARE(saveWith(*page, "done"), QString());
        QCOMPARE(page->saving()->step(), GamesSaving::Step::NotSaved);
        QCOMPARE(page->saving()->reason(), GamesSaving::Reason::Usage);
    }

    // ---- a child who is logged in when the parent saves ----

    // Before any password, the page asks about a logged-in child with new
    // games to copy, and nothing else on it changes meanwhile.
    void aLoggedInChildIsAskedAboutFirst() {
        m_loggedIn = {"ada", "guardian"};
        const auto page = makePage();
        page->turn("1001");
        qputenv("CAIRN_TEST_ARGUMENTS", path("arguments").toLocal8Bit());
        page->save();
        QCOMPARE(m_logindAsked, 1);
        QCOMPARE(page->question()->step(), LogOutQuestion::Step::Choosing);
        QCOMPARE(page->question()->name(), "Ada");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Ready);
        QVERIFY(page->busy());
        QVERIFY(!page->canSave());
        page->turn("1003");
        page->putBack();
        page->openSteam("ada");
        page->save();
        page->lookForGames();
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QCOMPARE(m_logindAsked, 1);
        QVERIFY(!QFile::exists(path("arguments")));
        QVERIFY(m_started.isEmpty());
        QCOMPARE(states(*page), QStringList({"1001=added", "1002=on", "1003=off"}));
    }

    // "Later" saves as before: nobody is logged out.
    void laterLogsNobodyOut() {
        m_loggedIn = {"ada"};
        const auto page = makePage();
        page->turn("1001");
        QCOMPARE(saveWith(*page, "done", false, [](LogOutQuestion& question) { question.later(); }),
                 QStringLiteral("run\nsave-games\nada=1002,1001\n"));
        QVERIFY(!page->question()->asking());
    }

    // "Log Ada out now", then "Log Ada out": one run, with one password,
    // that names her to log out.
    void logOutNowIsOneSave() {
        m_loggedIn = {"ada", "ben"};
        const auto page = makePage();
        page->turn("1001");
        page->choose("ben");
        page->turn("1003");
        const QString arguments = saveWith(*page, "save-done", false, [](LogOutQuestion& question) {
            question.logOutNow();
            question.logOut();
            QCOMPARE(question.name(), "Ben");
            question.logOutNow();
            question.notNow();
            question.later();
        });
        QCOMPARE(arguments,
                 QStringLiteral("run\nsave-games\n--log-out=ada\nada=1002,1001\nben=1003\n"));
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        QVERIFY(page->saving()->allGiven());
        QCOMPARE(page->saving()->results().constFirst().toMap().value("added").toInt(), 2);
    }

    // A child who is not logged in is not asked about, and the Save starts
    // at once.
    void aChildNotLoggedInIsNotAskedAbout() {
        m_loggedIn = {"ben", "guardian"};
        const auto page = makePage();
        page->turn("1001");
        QCOMPARE(saveWith(*page, "done"), QStringLiteral("run\nsave-games\nada=1002,1001\n"));
        QCOMPARE(m_logindAsked, 1);
    }

    // Taking a game off copies nothing, so nobody is asked about and logind
    // is not asked.
    void takingAGameOffAsksNothing() {
        m_loggedIn = {"ada"};
        const auto page = makePage();
        page->turn("1002");
        QCOMPARE(saveWith(*page, "done"), QStringLiteral("run\nsave-games\nada=\n"));
        QCOMPARE(m_logindAsked, 0);
    }

    // While the child is logged out, the page says whose log-out it is.
    void aLogOutInProgressNamesTheChild() {
        m_loggedIn = {"ada"};
        const auto page = makePage();
        page->turn("1001");
        qputenv("CAIRN_TEST_SCENARIO", "save-logging-out");
        page->save();
        page->question()->logOutNow();
        page->question()->logOut();
        QTRY_COMPARE_WITH_TIMEOUT(page->saving()->progress(), GamesSaving::Progress::LoggingOut,
                                  kAnswerMilliseconds);
        QCOMPARE(page->saving()->progressName(), "Ada");
        QVERIFY(page->busy());
    }

    // A program of the child's would not close: held as stuck, and the new
    // games wait for the child's next logout.
    void aStuckLogOutWaits() {
        m_loggedIn = {"ada"};
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-stuck", false, [](LogOutQuestion& question) {
            question.logOutNow();
            question.logOut();
        });
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        const QVariantMap ada = page->saving()->results().constFirst().toMap();
        QCOMPARE(ada.value("held").toInt(), static_cast<int>(GamesSaving::Held::Stuck));
        QCOMPARE(ada.value("later").toBool(), true);
        QVERIFY(page->chosenWaiting());
        QVERIFY(!page->canSave());
    }

    void aMissingCommandSaysItDidNotStart() {
        GamesPage::Setup setup;
        setup.pkexec = FIXTURES "/pkexec-family";
        setup.command = FIXTURES "/no-such-cairn-family";
        setup.catalogue = path("games.json");
        setup.lists.childLists = path("games");
        setup.accounts = [this] { return m_accounts; };
        setup.loggedIn = [] { return QSet<QString>(); };
        GamesPage page(setup);
        page.turn("1001");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is not there to run"));
        QCOMPARE(saveWith(page, "done"), QString());
        QCOMPARE(page.saving()->reason(), GamesSaving::Reason::CommandNotStarted);
        QVERIFY(page.canSave());
    }
};

} // namespace

QTEST_GUILESS_MAIN(GamesPageTest)
#include "tst_gamespage.moc"
