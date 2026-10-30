// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "ChildList.h"
#include "ChildLogOut.h"
#include "GameChoices.h"
#include "GameShelf.h"
#include "GamesSaving.h"
#include "LoginSessions.h"
#include "LoginWatch.h"
#include "SteamWindowStart.h"

#include <QDBusConnection>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

// What the Guardian tool's Games page shows and does (ADR-0041): the
// family's children, and for the one the parent picks, every game in the
// shared folder with the games on that child's list marked. The parent
// puts games on and takes them off, for one child or several, and Save
// gives each changed child their whole new list with one run of
// cairn-family save-games through pkexec. A child who is logged in has
// their games greyed, with a way to log them out first: one rule, so the
// parent never chooses games that could not be copied. It also opens a
// child's own Steam with the window the "Steam for <child>" menu entries
// open (ADR-0032).
class GamesPage : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The window is given one by main().")
    // Each child as a map: login and name, in the order their accounts
    // were made. It changes only when a child comes, goes or is renamed,
    // so the page keeps its controls, and keyboard focus, otherwise.
    Q_PROPERTY(QVariantList listedChildren READ listedChildren NOTIFY listedChildrenChanged)
    // The login of the child picked, or "" when there are no children.
    Q_PROPERTY(QString chosen READ chosen NOTIFY changed)
    Q_PROPERTY(QString chosenName READ chosenName NOTIFY changed)
    // The child picked is logged in, as logind says now.
    Q_PROPERTY(bool chosenLoggedIn READ chosenLoggedIn NOTIFY changed)
    // The child picked has their games greyed: they are logged in, or the
    // logout, its warning or its outcome is about them.
    Q_PROPERTY(bool chosenCovered READ chosenCovered NOTIFY changed)
    // Each game as a map: appId, title, picture (a file:// URL, or empty),
    // saved, when it is on the child's list as last saved, and shared, for
    // the child picked. The games on their saved list that the shared folder no
    // longer has come last, with shared false. Like listedChildren, it
    // changes only when one of those does, so turning a game keeps the
    // page's controls, and keyboard focus.
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    // The appids on the picked child's list as the parent wants it, with
    // the changes not saved yet.
    Q_PROPERTY(QStringList wanted READ wanted NOTIFY changed)
    Q_PROPERTY(List list READ list NOTIFY changed)
    Q_PROPERTY(int onListCount READ onListCount NOTIFY changed)
    // How many games are in the shared folder.
    Q_PROPERTY(int sharedCount READ sharedCount NOTIFY changed)
    // How many games on the child's list the shared folder no longer has.
    Q_PROPERTY(int goneCount READ goneCount NOTIFY changed)
    // The games in the Guardian's own Steam libraries, not the shared
    // folder, by name, as one list in words, such as "Celeste and Portal
    // 2", or "" when there are none. Children cannot have them until the
    // Guardian moves them (#167).
    Q_PROPERTY(QString ownSteamGames READ ownSteamGames NOTIFY changed)
    // The name of the child whose Steam window did not start, or "".
    Q_PROPERTY(QString steamNotOpened READ steamNotOpened NOTIFY changed)
    // The logins of the children with changes not saved yet, in the
    // children's order, and their names as one list in words, such as
    // "Ada and Ben".
    Q_PROPERTY(QStringList unsavedLogins READ unsavedLogins NOTIFY changed)
    Q_PROPERTY(QString unsavedNames READ unsavedNames NOTIFY changed)
    // The names, in words, of the children with changes not saved yet who
    // are logged in, which keeps Save off.
    Q_PROPERTY(QString loggedInUnsavedNames READ loggedInUnsavedNames NOTIFY changed)
    // Whether Save would save something now: never while a child with
    // changes is logged in.
    Q_PROPERTY(bool canSave READ canSave NOTIFY changed)
    // Whether "Put back as saved" would change the picked child's list now.
    Q_PROPERTY(bool canPutBack READ canPutBack NOTIFY changed)
    // A Save, a look for new games or a logout is running; every other
    // control waits.
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(GamesSaving* saving READ saving CONSTANT)
    Q_PROPERTY(ChildLogOut* loggingOut READ loggingOut CONSTANT)

public:
    // The picked child's own list, as GameShelf reads it.
    enum class List : std::uint8_t { None, Read, Unreadable };
    Q_ENUM(List)

    struct Setup {
        QString pkexec = QStringLiteral("/usr/bin/pkexec");
        // cairn-family; main() names the one this build installs.
        QString command;
        QString catalogue = QStringLiteral("/var/lib/cairn/games.json");
        QString childLists = QStringLiteral("/var/lib/cairn/games");
        // cairn-steam-for; main() names the one this build installs.
        QString steamFor;
        std::function<std::vector<Account>()> accounts = listAccounts;
        // Starts a program on its own, so it outlives this window.
        SteamWindowStart::Start start = SteamWindowStart::startOnItsOwn;
        // Who is logged in, and the bus logind says so on: the system bus
        // in main(), none in a test, which reads again by hand.
        LoginWatch::Read loggedIn = LoginSessions::loggedInNow;
        std::optional<QDBusConnection> loginBus;
        // The Guardian's home, whose own Steam is looked in for games
        // outside the shared folder: none when it is "", as in a test that
        // does not give one. main() gives the Guardian's own.
        QString steamHome;
        QString sharedLibrary = QStringLiteral("/var/lib/cairn/steam-library");
    };

    explicit GamesPage(Setup setup, QObject* parent = nullptr);

    QVariantList listedChildren() const;
    QString chosen() const;
    QString chosenName() const;
    bool chosenLoggedIn() const;
    bool chosenCovered() const;
    QVariantList games() const;
    QStringList wanted() const;
    List list() const;
    int onListCount() const;
    int sharedCount() const;
    int goneCount() const;
    QString ownSteamGames() const;
    QString steamNotOpened() const;
    QStringList unsavedLogins() const;
    QString unsavedNames() const;
    QString loggedInUnsavedNames() const;
    bool canSave() const;
    bool canPutBack() const;
    bool busy() const;
    GamesSaving* saving() const;
    ChildLogOut* loggingOut() const;

    // Picking a child, turning a game and putting a list back each end the
    // last Save's outcome, which was about the page as it was then.

    // Picks the child whose games show. A login that is not a listed child
    // changes nothing. Changes to the other children's lists are kept.
    Q_INVOKABLE void choose(const QString& login);
    // Puts a game on the picked child's list, or takes it off, until Save.
    // Only a game the page shows, not while a Save runs, and not while the
    // child's games are greyed.
    Q_INVOKABLE void turn(const QString& appId);
    // The picked child's list goes back to how it is saved.
    Q_INVOKABLE void putBack();
    // Saves every child's changed list, with one password.
    Q_INVOKABLE void save();
    // Makes the full games list and the pictures again, with one password,
    // so a game just installed shows. It names no child, so it changes no
    // child's list.
    Q_INVOKABLE void lookForGames();
    // Opens the child's own Steam, in the Steam window, as a program of its
    // own. A login that is not a listed child, or is logged in, opens
    // nothing.
    Q_INVOKABLE void openSteam(const QString& login);
    // Shows the warning before logging out the picked child, when they are
    // logged in and nothing else runs. loggingOut takes it from there.
    Q_INVOKABLE void logOut();
    // Reads the children and the games files again. Changes not saved yet
    // stay, but for a child who is gone or a game that can no longer show.
    // Who is logged in, and the Guardian's own Steam, are read again too.
    Q_INVOKABLE void reload();

signals:
    void listedChildrenChanged();
    void gamesChanged();
    void changed();

private:
    void readShelf();

    Setup m_setup;
    GamesSaving* m_saving;
    ChildLogOut* m_logOut;
    LoginWatch* m_logins;
    std::vector<ListedChild> m_children;
    QString m_chosen;
    GameShelf m_shelf;
    QVariantList m_games;
    GameChoices m_choices;
    QString m_steamNotOpened;
    QStringList m_ownSteamGames;
};
