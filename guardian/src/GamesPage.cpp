// SPDX-License-Identifier: Apache-2.0
#include "GamesPage.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QVariantMap>

#include <algorithm>
#include <utility>

GamesPage::GamesPage(Setup setup, QObject* parent) : QObject(parent), m_setup(std::move(setup)) {
    reload();
}

QVariantList GamesPage::listedChildren() const {
    QVariantList children;
    for (const ListedChild& child : m_children) {
        children.append(QVariantMap{
            {QStringLiteral("login"), child.login},
            {QStringLiteral("name"), child.name},
        });
    }
    return children;
}

QString GamesPage::chosen() const {
    return m_chosen;
}

QString GamesPage::chosenName() const {
    const ListedChild* picked = child(m_chosen);
    return picked != nullptr ? picked->name : QString();
}

QVariantList GamesPage::games() const {
    QVariantList games;
    for (const GameShelf::Game& game : m_shelf.games()) {
        games.append(QVariantMap{
            {QStringLiteral("appId"), game.appId},
            {QStringLiteral("title"), game.title},
            {QStringLiteral("picture"), game.picture},
            {QStringLiteral("onList"), game.onList},
        });
    }
    return games;
}

GamesPage::List GamesPage::list() const {
    switch (m_shelf.list()) {
    case GameShelf::List::Read:
        return List::Read;
    case GameShelf::List::Unreadable:
        return List::Unreadable;
    case GameShelf::List::None:
        break;
    }
    return List::None;
}

int GamesPage::onListCount() const {
    return m_shelf.onListCount();
}

QString GamesPage::steamNotOpened() const {
    return m_steamNotOpened;
}

void GamesPage::choose(const QString& login) {
    if (child(login) == nullptr || login == m_chosen) {
        return;
    }
    m_chosen = login;
    m_steamNotOpened.clear();
    readShelf();
    emit changed();
}

void GamesPage::openSteam(const QString& login) {
    const ListedChild* picked = child(login);
    if (picked == nullptr) {
        return;
    }
    const bool started = m_setup.start(m_setup.steamFor, {login});
    m_steamNotOpened = started ? QString() : picked->name;
    emit changed();
}

void GamesPage::reload() {
    std::vector<ListedChild> children = childrenAmong(m_setup.accounts());
    const bool sameChildren = std::ranges::equal(
        children, m_children, [](const ListedChild& one, const ListedChild& other) {
            return one.login == other.login && one.name == other.name;
        });
    m_children = std::move(children);
    // The child picked stays picked while they are on the list.
    if (child(m_chosen) == nullptr) {
        m_chosen = m_children.empty() ? QString() : m_children.front().login;
    }
    m_steamNotOpened.clear();
    readShelf();
    if (!sameChildren) {
        emit listedChildrenChanged();
    }
    emit changed();
}

bool GamesPage::startOnItsOwn(const QString& program, const QStringList& arguments) {
    return QProcess::startDetached(program, arguments);
}

const ListedChild* GamesPage::child(const QString& login) const {
    const auto found = std::ranges::find(m_children, login, &ListedChild::login);
    return found == m_children.end() ? nullptr : &*found;
}

void GamesPage::readShelf() {
    // The login goes into a file name, so only cairn-family's own rule for
    // a login may: anything else reads as a child with no list.
    static const QRegularExpression plain(QStringLiteral("^[a-z_][a-z0-9_-]{0,31}$"));
    const QString childList =
        plain.match(m_chosen).hasMatch()
            ? QDir(m_setup.childLists).filePath(m_chosen + QStringLiteral(".json"))
            : QString();
    m_shelf = GameShelf::read(m_setup.catalogue, childList);
}
