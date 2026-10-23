// SPDX-License-Identifier: Apache-2.0
#include "FamilyCommand.h"

#include "FamilyRole.h"
#include "SteamEntry.h"
#include "SystemTool.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <unistd.h>
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

unsigned int FamilyCommand::effectiveUid() {
    return geteuid();
}

std::optional<QString> FamilyCommand::pkexecUidFromEnvironment() {
    if (!qEnvironmentVariableIsSet("PKEXEC_UID")) {
        return std::nullopt;
    }
    return qEnvironmentVariable("PKEXEC_UID");
}

FamilyCommand::Outcome FamilyCommand::run(const QStringList& arguments) const {
    if (m_setup.euid != 0) {
        return {.refusal = Refusal::NotRoot, .asked = {}};
    }
    if (!callerIsAGrownUp()) {
        return {.refusal = Refusal::NotAGrownUp, .asked = {}};
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

// polkit has already asked for an administrator's password, but the command
// checks again, as the visit helper does (ADR-0032), so a mistake in a
// polkit rule cannot make it a child's. Root without pkexec is the family's
// setup, run with sudo.
bool FamilyCommand::callerIsAGrownUp() const {
    if (!m_setup.pkexecUid) {
        return true;
    }
    static const QRegularExpression number(QStringLiteral("^[0-9]{1,10}$"));
    bool converted = false;
    const unsigned int uid = m_setup.pkexecUid->toUInt(&converted);
    if (!number.match(*m_setup.pkexecUid).hasMatch() || !converted) {
        return false;
    }
    const std::optional<Account> caller = m_setup.lookUpUid(uid);
    return caller && FamilyRole::fromGroups(caller->groups).isAdministrator();
}

// The level group comes with the account, so a run cut off after useradd
// leaves a child the next run can finish, never an account it would refuse.
std::optional<Refusal> FamilyCommand::makeChild(const FamilyRequest& request) const {
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
