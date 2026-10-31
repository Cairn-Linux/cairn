// SPDX-License-Identifier: Apache-2.0
#include "AccountPage.h"
#include "ChildrenPage.h"
#include "GamesPage.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSet>
#include <QTemporaryDir>
#include <QtQuickTest/quicktest.h>

#include <algorithm>

namespace {

// A stand-in for a game's Steam header: a plain colour, different for each
// game, with the game's title on it, saved as `format`: JPEG, as the
// picture step copies one, or PNG, as Plasma's pictures are.
bool makePicture(const QString& path, const QString& title, int index, const char* format = "PNG") {
    QImage image(460, 215, QImage::Format_RGB32);
    image.fill(QColor::fromHsv((index * 47) % 360, 90, 200));
    QPainter painter(&image);
    QFont font = painter.font();
    font.setPixelSize(28);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(image.rect(), Qt::AlignCenter, title);
    painter.end();
    return image.save(path, format);
}

bool writeFile(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

QByteArray gamesFile(const QJsonArray& entries) {
    return QJsonDocument(QJsonObject{{"version", 1}, {"entries", entries}}).toJson();
}

// Makes the window's ChildrenPage for each test, over a family of two
// children and a Guardian, with the stand-ins for pkexec and cairn-family.
class StandInPages : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    // The scenario names what the stand-ins answer. An older child is a
    // third, at level 3. The window's QML owns what this returns.
    Q_INVOKABLE ChildrenPage* page(const QString& scenario, bool withChildren,
                                   bool withOlderChild = false) {
        qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
        std::vector<Account> accounts{
            {.uid = 1000,
             .groups = {"guardian", "wheel", "cairn-guardian"},
             .login = "guardian",
             .fullName = "Guardian"},
        };
        if (withChildren) {
            accounts.push_back(
                {.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"});
            accounts.push_back(
                {.uid = 1002, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = "Ben"});
        }
        if (withOlderChild) {
            accounts.push_back({.uid = 1003,
                                .groups = {"child", "cairn-l3"},
                                .login = "child",
                                .fullName = "A child with a long name"});
        }
        ChildrenPage::Setup setup;
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
        setup.command = QStringLiteral(FIXTURES "/cairn-family");
        setup.accounts = [accounts] { return accounts; };
        setup.nameInUse = [accounts](const QString& name) {
            return std::ranges::any_of(
                accounts, [&name](const Account& account) { return account.login == name; });
        };
        auto* page = new ChildrenPage(setup);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
        return page;
    }

    // The window's GamesPage over files of its own: eight shared games,
    // each with a picture but the last, in the pictures folder by its app
    // id (ADR-0047), Ada with three on her list, Ben with none and Cy with
    // two, one of them no longer shared, which has no picture. Its Steam
    // window start answers as `steamStarts` says, and is written down in
    // startedSteam(). Its Save runs the stand-ins, which answer as
    // `scenario` says, when one is given, and write the children's lists
    // into its folder. The children in `loggedIn` are logged in, until a
    // logout the stand-ins run, or setLoggedIn(), says otherwise. With
    // `withOwnSteamGames`, the Guardian's own Steam has two games and a
    // runtime outside the shared folder.
    Q_INVOKABLE GamesPage* gamesPage(bool withChildren, bool withGames, bool steamStarts = true,
                                     const QString& scenario = QString(),
                                     const QStringList& loggedIn = QStringList(),
                                     bool withOwnSteamGames = false) {
        const QDir files(m_files.path());
        setLoggedIn(loggedIn);
        QDir(files.filePath("pictures")).removeRecursively();
        files.mkpath("pictures");
        QJsonArray shared;
        const QStringList titles{"Putt-Putt Joins the Parade",
                                 "Freddi Fish",
                                 "Pajama Sam",
                                 "Spy Fox",
                                 "<b>Backyard</b> Football",
                                 "Tux Racer",
                                 "Mini Golf",
                                 "A game with a long name that will not fit on one line"};
        for (int i = 0; withGames && i < titles.size(); ++i) {
            const QString appId = QString::number(1001 + i);
            const QJsonObject entry{{"id", "steam-" + appId},
                                    {"title", titles.at(i)},
                                    {"category", "games"},
                                    {"exec", QJsonArray{"steam", "-applaunch", appId}}};
            if (i < titles.size() - 1) {
                makePicture(files.filePath("pictures/" + appId + ".jpg"), titles.at(i), i, "JPEG");
            }
            shared.append(entry);
        }
        files.mkpath("games");
        writeFile(files.filePath("games.json"), gamesFile(shared));
        QJsonArray ada;
        QJsonArray cy;
        if (!shared.isEmpty()) {
            ada = {shared.at(0), shared.at(2), shared.at(3)};
            // One game on Cy's list that the shared folder no longer has.
            cy = {shared.at(5), QJsonObject{{"id", "steam-2001"},
                                            {"title", "Humongous Bowling"},
                                            {"category", "games"},
                                            {"exec", QJsonArray{"steam", "-applaunch", "2001"}}}};
        }
        writeFile(files.filePath("games/ada.json"), gamesFile(ada));
        writeFile(files.filePath("games/cy.json"), gamesFile(cy));
        QFile::remove(files.filePath("games/ben.json"));

        std::vector<Account> accounts{
            {.uid = 1000,
             .groups = {"guardian", "wheel", "cairn-guardian"},
             .login = "guardian",
             .fullName = "Guardian"},
        };
        if (withChildren) {
            accounts.push_back(
                {.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"});
            accounts.push_back(
                {.uid = 1002, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = "Ben"});
            accounts.push_back(
                {.uid = 1003, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        }
        m_startedSteam.clear();
        if (!scenario.isEmpty()) {
            qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
        }
        qputenv("CAIRN_TEST_GAMES", files.filePath("games").toLocal8Bit());
        qputenv("CAIRN_TEST_LOGGED_IN", loggedInFile().toLocal8Bit());
        GamesPage::Setup setup;
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
        setup.command = QStringLiteral(FIXTURES "/cairn-family");
        setup.catalogue = files.filePath("games.json");
        setup.childLists = files.filePath("games");
        setup.steamFor = "/usr/bin/cairn-steam-for";
        setup.steamHome = ownSteamHome(withOwnSteamGames);
        setup.sharedLibrary = files.filePath("steam-library");
        setup.pictures = files.filePath("pictures");
        setup.accounts = [accounts] { return accounts; };
        setup.loggedIn = [this] {
            QFile file(loggedInFile());
            if (!file.open(QIODevice::ReadOnly)) {
                return QSet<QString>();
            }
            const QStringList logins =
                QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
            return QSet<QString>(logins.begin(), logins.end());
        };
        setup.start = [this, steamStarts](const QString& program, const QStringList& arguments) {
            m_startedSteam.append(program + " " + arguments.join(' '));
            return steamStarts;
        };
        auto* page = new GamesPage(setup);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
        return page;
    }

    // The window's AccountPage, for the Guardian of page(), with three
    // stand-in pictures of Plasma's. It has no bus, so a picture chosen is
    // never taken; its password runs the stand-ins as page()'s scenario
    // says.
    Q_INVOKABLE AccountPage* accountPage() {
        const QDir files(m_files.path());
        files.mkpath(QStringLiteral("avatars"));
        const QStringList names{"Artist Konqi", "Bookworm Konqi", "Katie"};
        for (int i = 0; i < names.size(); ++i) {
            makePicture(files.filePath("avatars/" + names.at(i) + ".png"), names.at(i), i * 3);
        }
        AccountPage::Setup setup;
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
        setup.command = QStringLiteral(FIXTURES "/cairn-family");
        setup.uid = 1000;
        setup.lookUpUid = [](unsigned int uid) -> std::optional<Account> {
            if (uid != 1000) {
                return std::nullopt;
            }
            return Account{.uid = 1000,
                           .groups = {"guardian", "wheel", "cairn-guardian"},
                           .login = "guardian",
                           .fullName = "Guardian"};
        };
        setup.stockPictures = files.filePath(QStringLiteral("avatars"));
        auto* page = new AccountPage(setup);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
        return page;
    }

    // Who logind says is logged in; the page reads it at its next reload.
    Q_INVOKABLE void setLoggedIn(const QStringList& logins) const {
        writeFile(loggedInFile(), logins.join('\n').toUtf8());
    }

    Q_INVOKABLE QStringList startedSteam() const {
        return m_startedSteam;
    }

    // Where to save a picture of the window, when one is wanted: the
    // Children page at this path, and the Games page beside it.
    Q_INVOKABLE QString screenshotPath() const {
        return qEnvironmentVariable("CAIRN_SCREENSHOT");
    }

private:
    // A home whose own Steam library has games in it, or "" for none.
    QString ownSteamHome(bool withGames) const {
        const QDir files(m_files.path());
        const QString steamapps = files.filePath("home/.local/share/Steam/steamapps");
        QDir(files.filePath("home")).removeRecursively();
        if (!withGames) {
            return {};
        }
        files.mkpath(steamapps);
        const QStringList manifests{
            R"("AppState" { "appid" "620" "name" "Portal 2" "installdir" "Portal 2" })",
            R"("AppState" { "appid" "504230" "name" "Celeste" "installdir" "Celeste" })",
            R"m("AppState" { "appid" "1628350" "name" "Steam Linux Runtime 3.0 (sniper)"
                            "installdir" "SteamLinuxRuntime_sniper" })m",
        };
        for (const QString& manifest : manifests) {
            const QString appId = manifest.section(u'"', 5, 5);
            writeFile(steamapps + "/appmanifest_" + appId + ".acf", manifest.toUtf8());
        }
        return files.filePath("home");
    }

    QString loggedInFile() const {
        return QDir(m_files.path()).filePath("logged-in");
    }

    QTemporaryDir m_files;
    QStringList m_startedSteam;
};

class Setup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        // The sentences under test are English, whatever the machine's
        // locale: an image build has none set, and Qt's own "C" joins names
        // with commas.
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        engine->rootContext()->setContextProperty(QStringLiteral("standIns"),
                                                  new StandInPages(engine));
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(familywindow, Setup)
#include "tst_familywindow.moc"
