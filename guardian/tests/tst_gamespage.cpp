// SPDX-License-Identifier: Apache-2.0
#include "GamesPage.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>
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
        setup.steamHome = m_steamHome;
        setup.sharedLibrary = path("steam-library");
        setup.accounts = [this] { return m_accounts; };
        setup.loggedIn = [this] { return loggedIn(); };
        setup.start = [this, starts](const QString& program, const QStringList& arguments) {
            m_started.append(program + " " + arguments.join(' '));
            return starts;
        };
        return std::make_unique<GamesPage>(setup);
    }

    // Who logind says is logged in: the logins in the file "logged-in",
    // which the stand-in empties when it logs a child out.
    QSet<QString> loggedIn() const {
        QFile file(path("logged-in"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        const QStringList logins =
            QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
        return {logins.begin(), logins.end()};
    }

    void setLoggedIn(const QStringList& logins) const {
        write(path("logged-in"), logins.join('\n').toUtf8());
    }

    // Logs the picked child out, past the warning, with the stand-ins
    // answering as the scenario says, and returns cairn-family's arguments.
    QString logOutWith(GamesPage& page, const QString& scenario) {
        QFile::remove(path("arguments"));
        qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
        qputenv("CAIRN_TEST_ARGUMENTS", path("arguments").toLocal8Bit());
        qputenv("CAIRN_TEST_LOGGED_IN", path("logged-in").toLocal8Bit());
        QSignalSpy ended(page.loggingOut(), &ChildLogOut::ended);
        page.logOut();
        page.loggingOut()->confirm();
        const bool answered = !ended.isEmpty() || ended.wait(kAnswerMilliseconds);
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        qunsetenv("CAIRN_TEST_LOGGED_IN");
        if (!answered) {
            return QStringLiteral("no answer");
        }
        QFile file(path("arguments"));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
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
    // each run starting with a line "run".
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

    // A game's manifest in the library `steamapps`, as Steam writes one.
    static void installIn(const QString& steamapps, const QString& appId, const QString& name) {
        QVERIFY(QDir().mkpath(steamapps));
        write(QDir(steamapps).filePath("appmanifest_" + appId + ".acf"),
              QString(R"("AppState" { "appid" "%1" "name" "%2" "installdir" "%2" })")
                  .arg(appId, name)
                  .toUtf8());
    }

    QTemporaryDir m_temporary;
    std::vector<Account> m_accounts;
    QStringList m_started;
    QString m_steamHome;

private slots:
    // The sentences under test are English, whatever the machine's locale:
    // an image build has none set, and Qt's own "C" joins names with commas.
    void initTestCase() {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void init() {
        QVERIFY(m_temporary.isValid());
        m_accounts = family();
        m_started.clear();
        m_steamHome.clear();
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

    // Games installed in the Guardian's own Steam, not the shared folder,
    // are named, but for Steam's tools and the games the family has.
    void gamesInTheGuardiansOwnSteam() {
        m_steamHome = path("home");
        const QString own = path("home/.local/share/Steam/steamapps");
        installIn(own, "620", "Portal 2");
        installIn(own, "504230", "Celeste");
        installIn(own, "1628350", "Steam Linux Runtime 3.0 (sniper)");
        installIn(own, "1001", "Game one");
        installIn(path("steam-library/steamapps"), "440", "Team Fortress 2");
        const auto page = makePage();
        QCOMPARE(page->ownSteamGames(), "Celeste and Portal 2");
        // Moved to the shared folder, it is named no more.
        QVERIFY(QFile::remove(own + "/appmanifest_620.acf"));
        page->reload();
        QCOMPARE(page->ownSteamGames(), "Celeste");
    }

    // Without a home to look in, as in every other test, nothing is named.
    void noHomeNoOwnSteamGames() {
        installIn(path("home/.local/share/Steam/steamapps"), "620", "Portal 2");
        const auto page = makePage();
        QCOMPARE(page->ownSteamGames(), QString());
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
        QVERIFY(page->saving()->allGiven());
    }

    // A child who is logged in gets only the games taken off; their new
    // games stay chosen, so Save can give them once they log out.
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

    // A copy refused because a child is being logged out is not a visit
    // the parent could close.
    void aCopyDuringALogoutIsHeldForIt() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-held-logging-out");
        const QVariantMap ada = page->saving()->results().constFirst().toMap();
        QCOMPARE(ada.value("held").toInt(), static_cast<int>(GamesSaving::Held::ChildLoggingOut));
        QVERIFY(page->canSave());
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

    // ---- Steam's tools (ADR-0046) ----

    void eachResultSaysWhatTheCopyDidWithTheTools_data() {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<GamesSaving::Tools>("tools");
        QTest::newRow("copied") << "save-tools" << GamesSaving::Tools::ToolsCopied;
        QTest::newRow("missing") << "save-tools-missing" << GamesSaving::Tools::ToolsMissing;
        QTest::newRow("none to copy") << "save-no-tools" << GamesSaving::Tools::NoToolsToCopy;
        QTest::newRow("nothing to say") << "save-done" << GamesSaving::Tools::NoToolNews;
    }

    // Each child's result says what their copy did with Steam's tools. A
    // tool never holds a child's games back.
    void eachResultSaysWhatTheCopyDidWithTheTools() {
        QFETCH(const QString, scenario);
        QFETCH(const GamesSaving::Tools, tools);
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, scenario);
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        const QVariantMap ada = page->saving()->results().constFirst().toMap();
        QCOMPARE(ada.value("tools").toInt(), static_cast<int>(tools));
        QVERIFY(page->saving()->allGiven());
        QVERIFY(!page->saving()->toolsLeftOut());
    }

    // A tool left out of the tools folder is worth saying after a Save that
    // gave a child something, and never after a look, which gives none.
    void aToolLeftOutIsSaidOnlyForASave() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-tools-left-out");
        QVERIFY(page->saving()->toolsLeftOut());
        page->saving()->close();
        QVERIFY(!page->saving()->toolsLeftOut());
        saveWith(*page, "save-tools-left-out", true);
        QCOMPARE(page->saving()->kind(), GamesSaving::Kind::Look);
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Saved);
        QVERIFY(!page->saving()->toolsLeftOut());
    }

    // A Save whose every copy was held gave no child a tool, so a tools
    // folder that was kept is nothing to say.
    void aHeldCopyHasNoToolLeftOut() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-tools-kept-but-held");
        QVERIFY(!page->saving()->toolsLeftOut());
        QCOMPARE(page->saving()->results().constFirst().toMap().value("tools").toInt(),
                 static_cast<int>(GamesSaving::Tools::NoToolNews));
    }

    void progressWhileTheToolsAreTaken() {
        const auto page = makePage();
        page->turn("1001");
        qputenv("CAIRN_TEST_SCENARIO", "save-sharing-tools");
        page->save();
        QTRY_COMPARE_WITH_TIMEOUT(page->saving()->progress(), GamesSaving::Progress::SharingTools,
                                  kAnswerMilliseconds);
        QCOMPARE(page->saving()->progressName(), QString());
        QVERIFY(page->busy());
    }

    void progressWhileAChildsToolsAreCopied() {
        const auto page = makePage();
        page->turn("1001");
        qputenv("CAIRN_TEST_SCENARIO", "save-tools-copying");
        page->save();
        QTRY_COMPARE_WITH_TIMEOUT(page->saving()->progress(), GamesSaving::Progress::CopyingTools,
                                  kAnswerMilliseconds);
        QCOMPARE(page->saving()->progressName(), "Ada");
        // The games copied so far still show; a tool is never one of them.
        QCOMPARE(page->saving()->copiedSoFar().size(), 1);
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

    // Each game copied, with its title and the child's name, for the
    // Save's progress window.
    void copiedGamesAreNamed() {
        const auto page = makePage();
        page->turn("1001");
        saveWith(*page, "save-done");
        const QVariantList copied = page->saving()->copiedSoFar();
        QCOMPARE(copied.size(), 2);
        const QVariantMap first = copied.at(0).toMap();
        QCOMPARE(first.value("login").toString(), "ada");
        QCOMPARE(first.value("name").toString(), "Ada");
        QCOMPARE(first.value("appId").toString(), "1002");
        QCOMPARE(first.value("title").toString(), "Game two");
        QCOMPARE(first.value("had").toBool(), false);
        // Close puts the outcome away, as the next change does.
        page->saving()->close();
        QCOMPARE(page->saving()->step(), GamesSaving::Step::Ready);
        QVERIFY(page->saving()->copiedSoFar().isEmpty());
    }

    // ---- a child who is logged in ----

    // Their games wait: nothing turns, nothing goes back, no Steam opens.
    void aLoggedInChildsGamesWait() {
        setLoggedIn({"ada"});
        const auto page = makePage();
        QVERIFY(page->chosenLoggedIn());
        QVERIFY(page->chosenCovered());
        page->turn("1001");
        page->openSteam("ada");
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=on", "1003=off"}));
        QVERIFY(m_started.isEmpty());
        QVERIFY(!page->canSave());
        // Ben is not logged in.
        page->choose("ben");
        QVERIFY(!page->chosenLoggedIn());
        QVERIFY(!page->chosenCovered());
        page->turn("1001");
        QVERIFY(page->canSave());
    }

    // A child who logs in with changes waiting keeps them, but Save waits
    // for every changed child to be logged out: one rule, removals too.
    void aChildWhoLogsInKeepsTheirChangesButSaveWaits() {
        const auto page = makePage();
        page->turn("1002");
        page->choose("ben");
        page->turn("1003");
        QVERIFY(page->canSave());
        const QSignalSpy changed(page.get(), &GamesPage::changed);
        setLoggedIn({"ada"});
        page->reload();
        QVERIFY(changed.count() >= 1);
        QVERIFY(!page->canSave());
        QCOMPARE(page->loggedInUnsavedNames(), "Ada");
        QCOMPARE(page->unsavedLogins(), QStringList({"ada", "ben"}));
        page->choose("ada");
        QVERIFY(!page->canPutBack());
        QCOMPARE(states(*page), QStringList({"1001=off", "1002=taken", "1003=off"}));
        setLoggedIn({});
        page->reload();
        QVERIFY(page->canSave());
        QCOMPARE(page->loggedInUnsavedNames(), QString());
    }

    // Log out warns first, then runs log-out for the picked child alone,
    // and the page reads who is logged in again when it ends.
    void logOutRunsLogOutForThePickedChild() {
        setLoggedIn({"ada", "ben"});
        const auto page = makePage();
        QCOMPARE(logOutWith(*page, "log-out-done"), QStringLiteral("run\nlog-out\nada\n"));
        QCOMPARE(page->loggingOut()->step(), ChildLogOut::Step::NotAsking);
        QVERIFY(!page->chosenLoggedIn());
        QVERIFY(!page->chosenCovered());
        page->turn("1001");
        QVERIFY(page->canSave());
    }

    void logOutOnlyForALoggedInChild() {
        const auto page = makePage();
        page->logOut();
        QCOMPARE(page->loggingOut()->step(), ChildLogOut::Step::NotAsking);
    }

    // While the logout runs, everything waits, Save and a look too.
    void everythingWaitsWhileALogoutRuns() {
        setLoggedIn({"ada"});
        const auto page = makePage();
        page->choose("ben");
        page->turn("1001");
        page->choose("ada");
        qputenv("CAIRN_TEST_SCENARIO", "log-out-running");
        page->logOut();
        QCOMPARE(page->loggingOut()->step(), ChildLogOut::Step::Warning);
        QVERIFY(page->chosenCovered());
        page->loggingOut()->confirm();
        QVERIFY(page->busy());
        QVERIFY(!page->canSave());
        qputenv("CAIRN_TEST_ARGUMENTS", path("arguments").toLocal8Bit());
        page->save();
        page->lookForGames();
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        QVERIFY(!QFile::exists(path("arguments")));
    }

    // A program that would not close keeps the card up, with the reason,
    // until the parent closes it.
    void aStuckLogoutSaysSo() {
        setLoggedIn({"ada"});
        const auto page = makePage();
        logOutWith(*page, "log-out-held-stuck");
        QCOMPARE(page->loggingOut()->step(), ChildLogOut::Step::NotLoggedOut);
        QCOMPARE(page->loggingOut()->reason(), ChildLogOut::Reason::Stuck);
        setLoggedIn({});
        page->reload();
        QVERIFY(page->chosenCovered());
        page->loggingOut()->close();
        QVERIFY(!page->chosenCovered());
    }

    // Picking another child puts a warning away.
    void pickingAnotherChildEndsTheWarning() {
        setLoggedIn({"ada"});
        const auto page = makePage();
        page->logOut();
        page->choose("ben");
        QCOMPARE(page->loggingOut()->step(), ChildLogOut::Step::NotAsking);
    }

    void aMissingCommandSaysItDidNotStart() {
        GamesPage::Setup setup;
        setup.pkexec = FIXTURES "/pkexec-family";
        setup.command = FIXTURES "/no-such-cairn-family";
        setup.catalogue = path("games.json");
        setup.childLists = path("games");
        setup.accounts = [this] { return m_accounts; };
        setup.loggedIn = [this] { return loggedIn(); };
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
