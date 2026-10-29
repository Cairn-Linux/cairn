// SPDX-License-Identifier: Apache-2.0
#include "FamilyCommand.h"

#include "FamilyRole.h"
#include "RootFolder.h"
#include "SteamEntry.h"
#include "SystemTool.h"

#include <QFileInfo>

#include <utility>

namespace {

QString askedFor(const FamilyRequest& request) {
    const QString action = request.action == FamilyRequest::Action::SetLevel
                               ? QStringLiteral("set-level")
                               : QStringLiteral("create-child");
    return QStringLiteral("%1 %2 at level %3").arg(action, request.login).arg(request.level);
}

} // namespace

FamilyCommand::FamilyCommand(Setup setup) : m_setup(std::move(setup)) {}

FamilyCommand::Outcome FamilyCommand::run(const QStringList& arguments) const {
    // This command reads no home, so only the check matters.
    const Caller caller = Caller::check(m_setup.euid, m_setup.pkexecUid, m_setup.lookUpUid,
                                        [](unsigned int) { return std::nullopt; });
    if (caller.refusal) {
        return {.refusal = caller.refusal, .asked = {}};
    }
    const FamilyRequest request = FamilyRequest::fromArguments(arguments);
    if (request.refusal) {
        return {.refusal = request.refusal, .asked = {}};
    }

    const QString asked = askedFor(request);
    const std::optional<Account> account = m_setup.lookUp(request.login);
    if (request.action == FamilyRequest::Action::SetLevel) {
        if (!account || !account->isCairnChild()) {
            return {.refusal = Refusal::NotAChild, .asked = asked};
        }
        return {.refusal = changeChild(*account, request), .asked = asked};
    }
    if (!account) {
        return {.refusal = makeChild(request), .asked = asked};
    }
    // An account Cairn did not make is never quietly made a child: it would
    // keep whatever it had before (#113).
    if (!account->isCairnChild()) {
        return {.refusal = Refusal::Taken, .asked = asked};
    }
    return {.refusal = changeChild(*account, request), .asked = asked};
}

// The level group comes with the account, so a run cut off after useradd
// leaves a child the next run can finish, never an account it would refuse.
// Games files of an account removed outside cairn-family go first: a
// waiting list would otherwise copy games no parent chose for this child.
std::optional<Refusal> FamilyCommand::makeChild(const FamilyRequest& request) const {
    if (!removeOldGames(request.login)) {
        return Refusal::Failed;
    }
    QStringList arguments{QStringLiteral("--create-home")};
    if (request.fullName) {
        arguments << QStringLiteral("--comment") << *request.fullName;
    }
    arguments << QStringLiteral("--groups") << FamilyRole::levelGroup(request.level)
              << request.login;
    if (!SystemTool::run(m_setup.useradd, arguments) ||
        !SystemTool::run(m_setup.passwd, {QStringLiteral("--lock"), request.login})) {
        return Refusal::Failed;
    }
    return writeEntry(request.login, request.fullName.value_or(request.login));
}

bool FamilyCommand::removeOldGames(const QString& login) const {
    return RootFolder::remove(m_setup.gamesFolder, m_setup.gamesOwner,
                              login + QStringLiteral(".json")) &&
           RootFolder::remove(m_setup.waitingFolder, m_setup.gamesOwner, login) &&
           RootFolder::remove(m_setup.finishedFolder, m_setup.gamesOwner, login);
}

std::optional<Refusal> FamilyCommand::changeChild(const Account& child,
                                                  const FamilyRequest& request) const {
    if (m_setup.passwordLocked(child.login) != true &&
        !SystemTool::run(m_setup.passwd, {QStringLiteral("--lock"), child.login})) {
        return Refusal::Failed;
    }
    QString name = child.displayName();
    if (request.fullName && *request.fullName != child.fullName) {
        if (!SystemTool::run(m_setup.usermod,
                             {QStringLiteral("--comment"), *request.fullName, child.login})) {
            return Refusal::Failed;
        }
        name = *request.fullName;
    }
    if (const std::optional<Refusal> refusal = setLevel(child, request.level)) {
        return refusal;
    }
    return writeEntry(child.login, name);
}

// The new level's group is added before the others are dropped, so a run cut
// off in between leaves the child in two groups, which every reader takes as
// the more restricted, and never in none, which the session would read as an
// account Cairn did not make (#100, ADR-0023).
std::optional<Refusal> FamilyCommand::setLevel(const Account& child, int level) const {
    const QString wanted = FamilyRole::levelGroup(level);
    if (!child.groups.contains(wanted) &&
        !SystemTool::run(m_setup.gpasswd, {QStringLiteral("--add"), child.login, wanted})) {
        return Refusal::Failed;
    }
    for (int other = FamilyRole::lowestLevel; other <= FamilyRole::highestLevel; ++other) {
        const QString group = FamilyRole::levelGroup(other);
        if (other != level && child.groups.contains(group) &&
            !SystemTool::run(m_setup.gpasswd, {QStringLiteral("--delete"), child.login, group})) {
            return Refusal::Failed;
        }
    }
    return std::nullopt;
}

// A file renamed into place keeps the label of the file it was written as,
// so the entry is labelled again for the folder, as the family's setup does.
std::optional<Refusal> FamilyCommand::writeEntry(const QString& login, const QString& name) const {
    const SteamEntry::Written written = SteamEntry::write(
        m_setup.applications, login, SteamEntry::text(login, name, m_setup.steamFor));
    if (written == SteamEntry::Written::Failed) {
        return Refusal::Failed;
    }
    if (written == SteamEntry::Written::Changed && QFileInfo(m_setup.restorecon).isExecutable() &&
        !SystemTool::run(m_setup.restorecon, {SteamEntry::path(m_setup.applications, login)})) {
        return Refusal::Failed;
    }
    return std::nullopt;
}
