// SPDX-License-Identifier: Apache-2.0
#include "GamesSave.h"

#include "ChildSave.h"
#include "GamesRequest.h"

#include <QLockFile>

#include <utility>
#include <vector>

GamesSave::GamesSave(Setup setup) : m_setup(std::move(setup)) {}

GamesSave::Outcome GamesSave::run(const QStringList& arguments, const Say& say) const {
    const Caller caller =
        Caller::check(m_setup.euid, m_setup.pkexecUid, m_setup.lookUpUid, m_setup.lookUpHome);
    if (caller.refusal) {
        return {.refusal = caller.refusal, .asked = {}, .children = {}};
    }
    const GamesRequest request = GamesRequest::fromArguments(arguments);
    if (request.refusal) {
        return {.refusal = request.refusal, .asked = {}, .children = {}};
    }
    const QString asked = QStringLiteral("save-games ") + request.logins().join(QLatin1Char(' '));

    // Every name is a child Cairn made, before anything changes.
    std::vector<Account> children;
    for (const GamesRequest::Child& wanted : request.children) {
        const std::optional<Account> account = m_setup.lookUp(wanted.login);
        if (!account || !account->isCairnChild()) {
            return {.refusal = Refusal::NotAChild, .asked = asked, .children = {}};
        }
        children.push_back(*account);
    }

    // No time limit: a Save lasts as long as its copies. A lock left by a
    // Save that was killed is taken back, because its process is gone.
    QLockFile lock(m_setup.lock);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        return {.refusal = Refusal::Busy, .asked = asked, .children = {}};
    }

    const CatalogueRefresh::Result made = CatalogueRefresh::run(m_setup.catalogue, caller.home);
    say(made.picturesMade ? QStringLiteral("pictures made") : QStringLiteral("pictures kept"));
    say(made.catalogueMade ? QStringLiteral("catalogue made") : QStringLiteral("catalogue kept"));

    const ChildSave::Shared shared{
        .copy = m_setup.copy,
        .file = ChildGamesFile(m_setup.gamesFolder, m_setup.gamesFolderOwner,
                               m_setup.catalogue.restorecon),
        .catalogue = made.catalogue.value_or(GameCatalogue()),
    };
    Outcome outcome{.refusal = std::nullopt, .asked = asked, .children = {}};
    for (std::size_t i = 0; i < children.size(); ++i) {
        outcome.children.append(ChildSave::run(
            shared, children[i], request.children.at(static_cast<qsizetype>(i)).appIds, say));
    }
    return outcome;
}
