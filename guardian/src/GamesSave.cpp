// SPDX-License-Identifier: Apache-2.0
#include "GamesSave.h"

#include "ChildSave.h"
#include "GamesRequest.h"

#include <QLockFile>

#include <utility>
#include <vector>

namespace {

const QStringList& tickedFor(const GamesRequest& request, std::size_t child) {
    return request.children.at(static_cast<qsizetype>(child)).appIds;
}

// A refusal changed nothing, so the journal gets only what was asked.
GamesSave::Outcome refused(Refusal refusal, const QString& asked) {
    return {.refusal = refusal,
            .asked = asked,
            .children = {},
            .pictures = {},
            .catalogue = {},
            .tools = {}};
}

// In the journal a bare `failed` or `refused folder` would read as the
// Save's own answer, so each of a step's own words says whose it is.
QStringList journal(const QString& line, const QString& step, const QStringList& notes) {
    QStringList lines{line};
    for (const QString& note : notes) {
        lines.append(step + QStringLiteral(": ") + note);
    }
    return lines;
}

} // namespace

GamesSave::GamesSave(Setup setup) : m_setup(std::move(setup)) {}

GamesSave::Outcome GamesSave::run(const QStringList& arguments, const Say& say) const {
    const Caller caller =
        Caller::check(m_setup.euid, m_setup.pkexecUid, m_setup.lookUpUid, m_setup.lookUpHome);
    if (caller.refusal) {
        return refused(*caller.refusal, {});
    }
    const GamesRequest request = GamesRequest::fromArguments(arguments);
    if (request.refusal) {
        return refused(*request.refusal, {});
    }
    const QString asked =
        (QStringList{QStringLiteral("save-games")} + request.logins()).join(QLatin1Char(' '));

    // Every name is a child Cairn made, before anything changes.
    std::vector<Account> children;
    for (const GamesRequest::Child& wanted : request.children) {
        const std::optional<Account> account = m_setup.lookUp(wanted.login);
        if (!account || !account->isCairnChild()) {
            return refused(Refusal::NotAChild, asked);
        }
        children.push_back(*account);
    }

    // No time limit: a Save lasts as long as its copies. A lock left by a
    // Save that was killed is taken back, because its process is gone.
    QLockFile lock(m_setup.lock);
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        return refused(Refusal::Busy, asked);
    }

    // The first line comes before the slow part, so the window knows the
    // password was accepted while the pictures and the full list are made.
    say(QStringLiteral("looking"));
    // The picture step keeps the pictures of the games on the lists this
    // Save writes, in the folder it writes them in.
    const CatalogueRefresh::Result made =
        CatalogueRefresh::run(m_setup.catalogue, caller.home, m_setup.gamesFolder);
    const QString picturesLine =
        made.picturesMade ? QStringLiteral("pictures made") : QStringLiteral("pictures kept");
    const QString catalogueLine =
        made.catalogueMade ? QStringLiteral("catalogue made") : QStringLiteral("catalogue kept");
    say(picturesLine);
    say(catalogueLine);
    for (const QString& line : made.withoutPicture) {
        say(line);
    }

    const ChildSave::Shared shared{
        .copy = m_setup.copy,
        .file = ChildGamesFile(m_setup.gamesFolder, m_setup.gamesFolderOwner,
                               m_setup.catalogue.restorecon),
        .catalogue = made.catalogue,
    };
    // Every child loses the games taken off before any copy starts, so a
    // Save cut off mid-copy still leaves no child a game the parent took off.
    std::vector<ChildSave::Before> before;
    before.reserve(children.size());
    for (std::size_t i = 0; i < children.size(); ++i) {
        before.push_back(ChildSave::takeOff(shared, children[i], tickedFor(request, i)));
    }
    // The copies take Steam's tools from the folder this fills. It comes
    // after the lists lose the games taken off, so a tools step that takes
    // its whole time limit still leaves every game taken off gone.
    const ToolsRefresh::Result tools = ToolsRefresh::run(m_setup.tools, caller.login);
    const QString toolsLine =
        tools.made ? QStringLiteral("tools made") : QStringLiteral("tools kept");
    say(toolsLine);
    for (const QString& line : tools.leftOut) {
        say(line);
    }
    Outcome outcome{
        .refusal = std::nullopt,
        .asked = asked,
        .children = {},
        .pictures = journal(picturesLine, QStringLiteral("picture step"), made.pictureNotes),
        .catalogue = journal(catalogueLine, QStringLiteral("catalogue step"),
                             made.catalogueMade ? QStringList() : QStringList{made.catalogueNote}),
        .tools = journal(toolsLine, QStringLiteral("tools step"), tools.notes)};
    for (std::size_t i = 0; i < children.size(); ++i) {
        outcome.children.append(
            ChildSave::run(shared, children[i], tickedFor(request, i), before[i], say));
    }
    return outcome;
}
