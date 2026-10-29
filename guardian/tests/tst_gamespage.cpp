// SPDX-License-Identifier: Apache-2.0
#include "GamesPage.h"

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

#include <memory>

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
        setup.childLists = path("games");
        setup.steamFor = "/usr/bin/cairn-steam-for";
        setup.accounts = [this] { return m_accounts; };
        setup.start = [this, starts](const QString& program, const QStringList& arguments) {
            m_started.append(program + " " + arguments.join(' '));
            return starts;
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
    // the scenario says, and returns the arguments cairn-family was given.
    QString saveWith(GamesPage& page, const QString& scenario, bool look = false) {
        QFile::remove(path("arguments"));
        qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
        qputenv("CAIRN_TEST_ARGUMENTS", path("arguments").toLocal8Bit());
        qputenv("CAIRN_TEST_GAMES", path("games").toLocal8Bit());
        QSignalSpy ended(page.saving(), &GamesSaving::ended);
        look ? page.lookForGames() : page.save();
        const bool answered = !ended.isEmpty() || ended.wait(kAnswerMilliseconds);
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        qunsetenv("CAIRN_TEST_GAMES");
        if (!answered) {
            return QStringLiteral("no answer");
        }
        QFile file(path("arguments"));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }

    QTemporaryDir m_temporary;
    std::vector<Account> m_accounts;
    QStringList m_started;

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        m_accounts = family();
        m_started.clear();
        write(path("games.json"), gamesFile({game("1001", "Game one"), game("1002", "Game two"),
                                             game("1003", "Game three")}));
        QVERIFY(QDir().mkpath(path("games")));
        write(path("games/ada.json"), gamesFile({game("1002", "Game two")}));
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
        page->turn("1003");
        page->putBack();
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
    }

    // A child who is logged in gets only the games taken off; their new
    // games stay chosen, so Save can give them later.
    void aLoggedInChildIsHeld() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-held-logged-in");
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        const QVariantMap ada = page->saving()->results().constFirst().toMap();
        QCOMPARE(ada.value("held").toInt(), static_cast<int>(GamesSaving::Held::LoggedIn));
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

    // Looking for new games saves the picked child's list as it is.
    void lookForGamesChangesNoList() {
        const auto page = makePage();
        page->choose("ben");
        page->turn("1003");
        page->choose("ada");
        QCOMPARE(saveWith(*page, "done", true), QStringLiteral("run\nsave-games\nada=1002\n"));
        QCOMPARE(page->saving()->kind(), GamesSaving::Kind::Look);
        QCOMPARE(page->unsavedLogins(), QStringList({"ben"}));
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

    void aMissingCommandSaysItCouldNotRead() {
        GamesPage::Setup setup;
        setup.pkexec = FIXTURES "/pkexec-family";
        setup.command = FIXTURES "/no-such-cairn-family";
        setup.catalogue = path("games.json");
        setup.childLists = path("games");
        setup.accounts = [this] { return m_accounts; };
        GamesPage page(setup);
        page.turn("1001");
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("is not there to run"));
        QCOMPARE(saveWith(page, "done"), QString());
        QCOMPARE(page.saving()->reason(), GamesSaving::Reason::Unknown);
        QVERIFY(page.canSave());
    }
};

} // namespace

QTEST_GUILESS_MAIN(GamesPageTest)
#include "tst_gamespage.moc"
