// SPDX-License-Identifier: Apache-2.0
#include "GamesSave.h"

#include "ChildGameList.h"
#include "ChildSave.h"
#include "FinishedSave.h"
#include "GamesRequest.h"
#include "WaitingSave.h"

#include <QLockFile>

#include <utility>
#include <vector>

namespace {

const QStringList& tickedFor(const GamesRequest& request, std::size_t child) {
    return request.children.at(static_cast<qsizetype>(child)).appIds;
}

// Writes down the child's whole new list as waiting when it has a game to
// copy, and says whether it did. The last copy at a logout no longer says
// anything true, so it goes, and so does an older waiting list.
bool writeDown(const ChildSave::Shared& shared, const WaitingSave& waiting,
               const FinishedSave& finished, const Account& child, const QStringList& ticked) {
    finished.remove(child.login);
    const ChildGameList::Plan plan = ChildGameList::plan(
        shared.file.read(child.login), ticked, shared.catalogue.value_or(GameCatalogue()));
    if (!plan.toCopy.isEmpty() && waiting.write(child.login, ticked)) {
        return true;
    }
    waiting.remove(child.login);
    return false;
}

} // namespace

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
    const QString asked =
        (QStringList{QStringLiteral("save-games")} + request.logins()).join(QLatin1Char(' '));

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

    // The first line comes before the slow part, so the window knows the
    // password was accepted while the pictures and the full list are made.
    say(QStringLiteral("looking"));
    const CatalogueRefresh::Result made = CatalogueRefresh::run(m_setup.catalogue, caller.home);
    say(made.picturesMade ? QStringLiteral("pictures made") : QStringLiteral("pictures kept"));
    say(made.catalogueMade ? QStringLiteral("catalogue made") : QStringLiteral("catalogue kept"));

    const ChildSave::Shared shared{
        .copy = m_setup.copy,
        .file = ChildGamesFile(m_setup.gamesFolder, m_setup.gamesFolderOwner,
                               m_setup.catalogue.restorecon),
        .catalogue = made.catalogue,
    };
    const WaitingSave waiting(m_setup.waitingFolder, m_setup.gamesFolderOwner,
                              m_setup.catalogue.restorecon);
    const FinishedSave finished(m_setup.finishedFolder, m_setup.gamesFolderOwner,
                                m_setup.catalogue.restorecon);
    // Every child's games to copy are written down before anything changes
    // for them, so a Save cut off from here on, by a logout or the power,
    // still copies them at the child's next logout.
    QList<bool> writtenDown;
    for (std::size_t i = 0; i < children.size(); ++i) {
        writtenDown.append(
            writeDown(shared, waiting, finished, children[i], tickedFor(request, i)));
    }
    // Every child loses the games taken off before any copy starts, so a
    // Save cut off mid-copy still leaves no child a game the parent took off.
    std::vector<ChildSave::Before> before;
    before.reserve(children.size());
    for (std::size_t i = 0; i < children.size(); ++i) {
        before.push_back(ChildSave::takeOff(shared, children[i], tickedFor(request, i)));
    }
    Outcome outcome{.refusal = std::nullopt, .asked = asked, .children = {}};
    for (std::size_t i = 0; i < children.size(); ++i) {
        const ChildSave::Ended ended =
            ChildSave::run(shared, children[i], tickedFor(request, i), before[i], say);
        outcome.children.append(ended.line);
        if (writtenDown.at(static_cast<qsizetype>(i)) && WaitingSave::keeps(ended.held)) {
            const QString later = QStringLiteral("later ") + children[i].login;
            say(later);
            outcome.children.append(later);
        } else {
            waiting.remove(children[i].login);
        }
    }
    return outcome;
}
