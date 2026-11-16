// SPDX-License-Identifier: Apache-2.0
#include "AboutPage.h"
#include "AccountPage.h"
#include "ChildrenPage.h"
#include "GamesPage.h"
#include "NoticesPage.h"
#include "StandInWallet.h"

#include <QAccessible>
#include <QColor>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLocale>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>
#include <QSet>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QUrl>
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

// One Steam game's entry in a games file.
QJsonObject steamGame(const QString& appId, const QString& title) {
    return {{"id", "steam-" + appId},
            {"title", title},
            {"category", "games"},
            {"exec", QJsonArray{"steam", "-applaunch", appId}}};
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
        m_accounts = {
            {.uid = 1000,
             .groups = {"guardian", "wheel", "cairn-guardian"},
             .login = "guardian",
             .fullName = "Guardian"},
        };
        if (withChildren) {
            m_accounts.push_back(
                {.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"});
            m_accounts.push_back(
                {.uid = 1002, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = "Ben"});
        }
        if (withOlderChild) {
            m_accounts.push_back({.uid = 1003,
                                  .groups = {"child", "cairn-l3"},
                                  .login = "child",
                                  .fullName = "A child with a long name"});
        }
        ChildrenPage::Setup setup;
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
        setup.command = QStringLiteral(FIXTURES "/cairn-family");
        // The table is this object's, which outlives every window, so a
        // test can change the family behind an open window's back.
        m_reads = 0;
        setup.accounts = [this] {
            ++m_reads;
            return m_accounts;
        };
        setup.nameInUse = [this](const QString& name) {
            return std::ranges::any_of(
                m_accounts, [&name](const Account& account) { return account.login == name; });
        };
        // Ben has British English, chosen in Family; Ada has none chosen.
        setup.languageOf = [](const QString& login) -> std::optional<QString> {
            if (login == QLatin1String("ben")) {
                return QStringLiteral("en_GB.UTF-8");
            }
            return std::nullopt;
        };
        auto* page = new ChildrenPage(setup);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
        return page;
    }

    // The account is gone from the Children page's table, as after
    // `userdel` in a terminal while the window is open.
    Q_INVOKABLE void removeAccount(const QString& login) {
        std::erase_if(m_accounts,
                      [&login](const Account& account) { return account.login == login; });
    }

    // A child's account with this login is in the table, last, at this
    // level, as after `cairn-family` in a terminal made or changed it.
    Q_INVOKABLE void putAccount(const QString& login, const QString& name, int level) {
        removeAccount(login);
        m_accounts.push_back({.uid = 1010,
                              .groups = {login, QStringLiteral("cairn-l%1").arg(level)},
                              .login = login,
                              .fullName = name});
    }

    // How many times the Children page has read the accounts.
    Q_INVOKABLE int accountReads() const {
        return m_reads;
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
            if (i < titles.size() - 1) {
                makePicture(files.filePath("pictures/" + appId + ".jpg"), titles.at(i), i, "JPEG");
            }
            shared.append(steamGame(appId, titles.at(i)));
        }
        m_shared = shared;
        files.mkpath("games");
        writeFile(files.filePath("games.json"), gamesFile(shared));
        QJsonArray ada;
        QJsonArray cy;
        if (!shared.isEmpty()) {
            ada = {shared.at(0), shared.at(2), shared.at(3)};
            // One game on Cy's list that the shared folder no longer has.
            cy = {shared.at(5), steamGame("2001", "Humongous Bowling")};
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

    // Takes kwalletd's name for the stand-in kwalletd, on the test's own
    // session bus. False when it cannot, as on a real session.
    bool startWallet() {
        return m_kwalletd.start();
    }

    // The window's AccountPage, for the Guardian of page(), with three
    // stand-in pictures of Plasma's. It has no AccountsService bus, so a
    // picture chosen is never taken; its password runs the stand-ins as
    // page()'s scenario says. Its kwalletd is the stand-in, with the
    // wallet pam_kwallet opens when `withWallet`, and with none otherwise.
    Q_INVOKABLE AccountPage* accountPage(bool withWallet = false) {
        m_kwalletd.reset();
        if (!withWallet) {
            m_kwalletd.walletNames.clear();
        }
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
        setup.walletBus = QDBusConnection::sessionBus();
        auto* page = new AccountPage(setup);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
        return page;
    }

    // The stand-in kwalletd answers `method` with an error from now on.
    Q_INVOKABLE void walletFails(const QString& method) {
        m_kwalletd.failing = method;
    }

    // The stand-in kwalletd holds its first answer until releaseWallet().
    Q_INVOKABLE void holdWallet() {
        m_kwalletd.holding = true;
    }

    Q_INVOKABLE void releaseWallet() {
        m_kwalletd.release();
    }

    // What the stand-in kwalletd was asked, in order.
    Q_INVOKABLE QStringList walletAsked() const {
        return m_kwalletd.asked;
    }

    // Who logind says is logged in; the page reads it at its next reload.
    Q_INVOKABLE void setLoggedIn(const QStringList& logins) const {
        writeFile(loggedInFile(), logins.join('\n').toUtf8());
    }

    // Games installed in the shared folder since the page read it: the
    // full list gets one more for each title, as the next look would make
    // it. The stand-ins never write the full list, so the page sees them
    // when it next reads everything, as at the end of a look.
    Q_INVOKABLE void installSharedGames(const QStringList& titles) const {
        QJsonArray shared = m_shared;
        for (int i = 0; i < titles.size(); ++i) {
            shared.append(steamGame(QString::number(1101 + i), titles.at(i)));
        }
        writeFile(QDir(m_files.path()).filePath("games.json"), gamesFile(shared));
    }

    // The window's NoticesPage, for the Guardian of page(), over a journal
    // of its own: "tries" has three wrong passwords at the login screen
    // that locked the Guardian, and one at su for a name typed with markup
    // in it, from Ben's login; "many" has 30 notices, more than fit;
    // "none" has nothing; "unreadable" is a
    // journal this account may not read. Times are in UTC.
    Q_INVOKABLE NoticesPage* noticesPage(const QString& journal = QStringLiteral("none")) {
        const QDir files(m_files.path());
        const QString id = QStringLiteral("0123456789abcdef0123456789abcdef");
        files.mkpath("notices/journal/" + id);
        writeFile(files.filePath("notices/machine-id"), id.toLatin1());
        const QString journalFile = files.filePath("notices/journal/" + id + "/system.journal");
        writeFile(journalFile, "LPKSHHRH");
        QFile::setPermissions(journalFile, journal == "unreadable"
                                               ? QFile::WriteOwner
                                               : QFile::ReadOwner | QFile::WriteOwner);
        QByteArray audit;
        if (journal == "tries") {
            const qint64 start = 1793808120LL * 1000000;
            for (int i = 0; i < 3; ++i) {
                audit += auditLine(start + i * 60000000LL, "guardian", "/usr/libexec/sddm-helper",
                                   "4294967295", 100 + i);
            }
            audit += auditLine(start + 7200000000LL, "<b>guardian</b>", "/usr/bin/su", "1002", 200);
        }
        if (journal == "many") {
            // A wrong su each 20 minutes: a notice each, more than fit.
            const qint64 start = 1793808120LL * 1000000;
            for (int i = 0; i < 30; ++i) {
                audit +=
                    auditLine(start + i * 1200000000LL, "guardian", "/usr/bin/su", "1002", 300 + i);
            }
        }
        writeFile(files.filePath("notices/audit"), audit);
        writeFile(files.filePath("notices/journalctl"),
                  "#!/bin/bash\ncase \"$*\" in\n  *_AUDIT_TYPE=1100*) cat '" +
                      files.filePath("notices/audit").toUtf8() + "' ;;\nesac\nexit 0\n");
        QFile::setPermissions(files.filePath("notices/journalctl"),
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        NoticesPage::Setup setup;
        setup.reader.journalctl = files.filePath("notices/journalctl");
        setup.reader.journalFolders = {files.filePath("notices/journal")};
        setup.reader.machineId = files.filePath("notices/machine-id");
        const std::vector<Account> accounts = family();
        setup.reader.lookups.byName = [accounts](const QString& name) -> std::optional<Account> {
            for (const Account& account : accounts) {
                if (account.login == name) {
                    return account;
                }
            }
            return std::nullopt;
        };
        setup.reader.lookups.byUid = [accounts](unsigned int uid) -> std::optional<Account> {
            for (const Account& account : accounts) {
                if (account.uid == uid) {
                    return account;
                }
            }
            return std::nullopt;
        };
        setup.uid = 1000;
        setup.faillockConf = QStringLiteral("/nonexistent/faillock.conf");
        setup.zone = QTimeZone::utc();
        auto* page = new NoticesPage(setup);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
        return page;
    }

    // The window's AboutPage, over an os-release and a folder of licence
    // texts of its own. "image" is the image as it is built today: Cairn's
    // file with no version of Cairn's in it, and the four full texts.
    // "with-version" has the image's version too; "not-cairn" is another
    // system's file; "no-licences" has no folder of full texts;
    // "nothing-opens" is "image" with an opener that answers no. A press
    // only adds its address to opened(): no test starts a web browser.
    Q_INVOKABLE AboutPage* aboutPage(const QString& kind = QStringLiteral("image")) {
        const QDir files(m_files.path());
        QFile image(QStringLiteral(FIXTURES "/os-release/cairn"));
        QByteArray osRelease = image.open(QIODevice::ReadOnly) ? image.readAll() : QByteArray();
        if (kind == "with-version") {
            osRelease += "IMAGE_VERSION=dev.20261112-0a1b2c3\n";
        }
        if (kind == "not-cairn") {
            osRelease = "NAME=\"Fedora Linux\"\nID=fedora\nVERSION_ID=45\n"
                        "IMAGE_VERSION=45.20261104\n";
        }
        QDir(files.filePath("about")).removeRecursively();
        files.mkpath("about");
        writeFile(files.filePath("about/os-release"), osRelease);
        if (kind != "no-licences") {
            files.mkpath("about/licenses/cairn");
            for (const char* text :
                 {"LICENSE", "LICENSE-CC-BY-SA-4.0", "NOTICE", "TRADEMARKS.md"}) {
                writeFile(files.filePath("about/licenses/cairn/" + QString::fromLatin1(text)),
                          "words\n");
            }
        }
        m_opened.clear();
        AboutPage::Setup setup;
        setup.osRelease = files.filePath("about/os-release");
        setup.licenceFolder = files.filePath("about/licenses/cairn");
        const bool opens = kind != "nothing-opens";
        setup.open = [this, opens](const QUrl& address) {
            m_opened.append(address.toString());
            return opens;
        };
        auto* page = new AboutPage(setup);
        QQmlEngine::setObjectOwnership(page, QQmlEngine::JavaScriptOwnership);
        return page;
    }

    // Each address a press on the About page asked the desktop to open.
    Q_INVOKABLE QStringList opened() const {
        return m_opened;
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
    static std::vector<Account> family() {
        return {
            {.uid = 1000,
             .groups = {"guardian", "wheel", "cairn-guardian"},
             .login = "guardian",
             .fullName = "Guardian"},
            {.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"},
            {.uid = 1002, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = "Ben"},
        };
    }

    // One failed password check, as journald keeps the audit record.
    static QByteArray auditLine(qint64 time, const QString& account, const QString& program,
                                const QString& loginUid, int pid) {
        return QJsonDocument(QJsonObject{{"__REALTIME_TIMESTAMP", QString::number(time)},
                                         {"_TRANSPORT", "audit"},
                                         {"_AUDIT_TYPE", "1100"},
                                         {"_PID", QString::number(pid)},
                                         {"_AUDIT_LOGINUID", loginUid},
                                         {"AUDIT_FIELD_OP", "PAM:authentication"},
                                         {"AUDIT_FIELD_ACCT", account},
                                         {"AUDIT_FIELD_EXE", program},
                                         {"AUDIT_FIELD_RES", "failed"}})
                   .toJson(QJsonDocument::Compact) +
               '\n';
    }

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
    // The accounts the Children page that page() last made reads.
    std::vector<Account> m_accounts;
    int m_reads = 0;
    // The shared games gamesPage() last wrote into the full list.
    QJsonArray m_shared;
    QStringList m_startedSteam;
    QStringList m_opened;
    StandInWallet m_kwalletd;
};

// TestCase.keyPress cannot mark a press as the keyboard's own repeat, which
// is what a grown-up holding a key down sends. The tests get it as `heldKey`.
class HeldKey : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    // One repeat of `key`, delivered to whatever has the focus in `window`.
    // A repeat of Space carries a space, as a keyboard's does, which a text
    // field would type.
    Q_INVOKABLE void repeat(QObject* window, int key) const {
        const QString text = key == Qt::Key_Space ? QStringLiteral(" ") : QString();
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, text, true);
        QCoreApplication::sendEvent(window, &event);
    }
};

// What a screen reader is offered on one control, and its press and
// toggle: the same calls an assistive tool makes, which no key or tap in a
// test reaches. The tests get it as `screenReader`.
class ScreenReader : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    // The names of the actions the control offers, such as "Press".
    Q_INVOKABLE QStringList actions(QObject* control) const {
        QAccessibleInterface* seen = QAccessible::queryAccessibleInterface(control);
        if (seen == nullptr || seen->actionInterface() == nullptr) {
            return {};
        }
        return seen->actionInterface()->actionNames();
    }

    // Asks the control to be pressed, whether or not it offers that.
    Q_INVOKABLE void press(QObject* control) const {
        ask(control, QAccessibleActionInterface::pressAction());
    }

    // Asks the control to be checked or unchecked, whether or not it
    // offers that.
    Q_INVOKABLE void toggle(QObject* control) const {
        ask(control, QAccessibleActionInterface::toggleAction());
    }

private:
    static void ask(QObject* control, const QString& action) {
        QAccessibleInterface* seen = QAccessible::queryAccessibleInterface(control);
        if (seen != nullptr && seen->actionInterface() != nullptr) {
            seen->actionInterface()->doAction(action);
        }
    }
};

class Setup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        // The sentences under test are English, whatever the machine's
        // locale: an image build has none set, and Qt's own "C" joins names
        // with commas.
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        auto* standIns = new StandInPages(engine);
        // Never ask a real kwalletd: on a real session its box would open.
        if (!standIns->startWallet()) {
            qFatal("kwalletd's name is taken: run under dbus-run-session, as CTest does, never on "
                   "a real session.");
        }
        engine->rootContext()->setContextProperty(QStringLiteral("standIns"), standIns);
        engine->rootContext()->setContextProperty(QStringLiteral("heldKey"), new HeldKey(engine));
        engine->rootContext()->setContextProperty(QStringLiteral("screenReader"),
                                                  new ScreenReader(engine));
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(familywindow, Setup)
#include "tst_familywindow.moc"
