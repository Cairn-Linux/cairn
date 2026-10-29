// SPDX-License-Identifier: Apache-2.0
#include "GamesPage.h"

#include <utility>

GamesPage::GamesPage(Setup setup, QObject* parent)
    : QObject(parent), m_setup(std::move(setup)),
      m_saving(new GamesSaving(m_setup.pkexec, m_setup.command, this)) {
    connect(m_saving, &GamesSaving::changed, this, &GamesPage::changed);
    // Whatever the answer, the lists are read again: a Save that stopped
    // part way may still have written some.
    connect(m_saving, &GamesSaving::ended, this, &GamesPage::reload);
    reload();
}

QVariantList GamesPage::listedChildren() const {
    return loginsAndNames(m_children);
}

QString GamesPage::chosen() const {
    return m_chosen;
}

QString GamesPage::chosenName() const {
    const ListedChild* picked = childWithLogin(m_children, m_chosen);
    return picked != nullptr ? picked->name : QString();
}

QVariantList GamesPage::games() const {
    return m_games;
}

QStringList GamesPage::wanted() const {
    return m_choices.wanted(m_chosen);
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
    return m_shelf.countAmong(wanted());
}

int GamesPage::sharedCount() const {
    return m_shelf.sharedCount();
}

int GamesPage::goneCount() const {
    return static_cast<int>(m_shelf.games().size()) - m_shelf.sharedCount();
}

QString GamesPage::steamNotOpened() const {
    return m_steamNotOpened;
}

QStringList GamesPage::unsavedLogins() const {
    return m_choices.changedAmong(loginsOf(m_children));
}

QString GamesPage::unsavedNames() const {
    return namesInWords(m_children, unsavedLogins());
}

bool GamesPage::canSave() const {
    return m_choices.anyChanged() && !busy();
}

bool GamesPage::canPutBack() const {
    return m_choices.changed(m_chosen) && !busy();
}

bool GamesPage::busy() const {
    return m_saving->busy();
}

GamesSaving* GamesPage::saving() const {
    return m_saving;
}

void GamesPage::choose(const QString& login) {
    if (childWithLogin(m_children, login) == nullptr || login == m_chosen) {
        return;
    }
    m_chosen = login;
    m_steamNotOpened.clear();
    m_saving->clearOutcome();
    readShelf();
    emit changed();
}

void GamesPage::turn(const QString& appId) {
    if (busy() || !m_shelf.shows(appId) || childWithLogin(m_children, m_chosen) == nullptr) {
        return;
    }
    m_choices.turn(m_chosen, appId);
    m_saving->clearOutcome();
    emit changed();
}

void GamesPage::putBack() {
    if (!canPutBack()) {
        return;
    }
    m_choices.putBack(m_chosen);
    m_saving->clearOutcome();
    emit changed();
}

void GamesPage::save() {
    if (canSave()) {
        m_saving->start(GamesSaving::Kind::Save, m_choices.saveArguments(loginsOf(m_children)),
                        m_children, m_shelf.games());
    }
}

// save-games with no child named: no list read by this window, which may
// be out of date or unreadable, is ever sent back to be written.
void GamesPage::lookForGames() {
    if (!busy()) {
        m_saving->start(GamesSaving::Kind::Look, {QStringLiteral("save-games")}, m_children,
                        m_shelf.games());
    }
}

void GamesPage::openSteam(const QString& login) {
    const ListedChild* picked = childWithLogin(m_children, login);
    if (picked != nullptr && !busy()) {
        const bool opened = SteamWindowStart::open(m_setup.steamFor, login, m_setup.start);
        m_steamNotOpened = opened ? QString() : picked->name;
        emit changed();
    }
}

void GamesPage::reload() {
    std::vector<ListedChild> children = childrenAmong(m_setup.accounts());
    const bool sameChildren = sameLoginsAndNames(children, m_children);
    m_children = std::move(children);
    // The child picked stays picked while they are on the list.
    if (childWithLogin(m_children, m_chosen) == nullptr) {
        m_chosen = m_children.empty() ? QString() : m_children.front().login;
    }
    m_steamNotOpened.clear();
    for (const QString& login : loginsOf(m_children)) {
        m_choices.setSaved(
            login, GameShelf::listedIds(GameShelf::childListPath(m_setup.childLists, login)));
    }
    readShelf();
    m_choices.keepOnly(loginsOf(m_children), m_shelf.sharedIds());
    if (!sameChildren) {
        emit listedChildrenChanged();
    }
    emit changed();
}

void GamesPage::readShelf() {
    m_shelf =
        GameShelf::read(m_setup.catalogue, GameShelf::childListPath(m_setup.childLists, m_chosen));
    const QVariantList games = m_shelf.toVariantList();
    if (games != m_games) {
        m_games = games;
        emit gamesChanged();
    }
}
