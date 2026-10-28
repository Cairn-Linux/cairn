// SPDX-License-Identifier: Apache-2.0
#include "GamesPage.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QVariantMap>

#include <memory>

namespace {

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
            if (game.value("onList").toBool()) {
                titles.append(game.value("title").toString());
            }
        }
        return titles;
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
        QCOMPARE(second.value("onList").toBool(), true);
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

    void noSharedGames() {
        QVERIFY(QFile::remove(path("games.json")));
        const auto page = makePage();
        QVERIFY(page->games().isEmpty());
        QCOMPARE(page->chosen(), "ada");
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
        page->openSteam("ben");
        QCOMPARE(page->steamNotOpened(), "Ben");
        // Picking another child clears it.
        page->choose("cy");
        QCOMPARE(page->steamNotOpened(), QString());
    }

    void openSteamOnlyForAChild() {
        const auto page = makePage();
        page->openSteam("guardian");
        page->openSteam("guest");
        page->openSteam("");
        QVERIFY(m_started.isEmpty());
    }
};

} // namespace

QTEST_GUILESS_MAIN(GamesPageTest)
#include "tst_gamespage.moc"
