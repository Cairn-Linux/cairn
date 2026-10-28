// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "ChildList.h"
#include "GameShelf.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <cstdint>
#include <functional>
#include <vector>

// What the Guardian tool's Games page shows (ADR-0041): the family's
// children, and for the one the parent picks, every game in the shared
// folder with the games on that child's own list marked. It only reads; it
// changes nothing and asks for no password. It also opens a child's own
// Steam with the window the "Steam for <child>" menu entries open
// (ADR-0032).
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
    // Each game in the shared folder as a map: appId, title, picture (a
    // file:// URL, or empty) and onList, for the child picked.
    Q_PROPERTY(QVariantList games READ games NOTIFY changed)
    Q_PROPERTY(List list READ list NOTIFY changed)
    Q_PROPERTY(int onListCount READ onListCount NOTIFY changed)
    // The name of the child whose Steam window did not start, or "".
    Q_PROPERTY(QString steamNotOpened READ steamNotOpened NOTIFY changed)

public:
    // The picked child's own list, as GameShelf reads it.
    enum class List : std::uint8_t { None, Read, Unreadable };
    Q_ENUM(List)

    struct Setup {
        QString catalogue = QStringLiteral("/var/lib/cairn/games.json");
        QString childLists = QStringLiteral("/var/lib/cairn/games");
        // cairn-steam-for; main() names the one this build installs.
        QString steamFor;
        std::function<std::vector<Account>()> accounts = listAccounts;
        // Starts a program on its own, so it outlives this window.
        std::function<bool(const QString&, const QStringList&)> start = startOnItsOwn;
    };

    explicit GamesPage(Setup setup, QObject* parent = nullptr);

    QVariantList listedChildren() const;
    QString chosen() const;
    QString chosenName() const;
    QVariantList games() const;
    List list() const;
    int onListCount() const;
    QString steamNotOpened() const;

    // Picks the child whose games show. A login that is not a listed child
    // changes nothing.
    Q_INVOKABLE void choose(const QString& login);
    // Opens the child's own Steam, in the Steam window, as a program of its
    // own. A login that is not a listed child opens nothing.
    Q_INVOKABLE void openSteam(const QString& login);
    // Reads the children and both games files again.
    Q_INVOKABLE void reload();

    static bool startOnItsOwn(const QString& program, const QStringList& arguments);

signals:
    void listedChildrenChanged();
    void changed();

private:
    const ListedChild* child(const QString& login) const;
    void readShelf();

    Setup m_setup;
    std::vector<ListedChild> m_children;
    QString m_chosen;
    GameShelf m_shelf;
    QString m_steamNotOpened;
};
