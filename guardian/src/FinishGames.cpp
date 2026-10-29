// SPDX-License-Identifier: Apache-2.0
#include "FinishGames.h"

#include "CatalogueRefresh.h"
#include "ChildGamesFile.h"
#include "ChildSave.h"
#include "FinishedSave.h"
#include "RootFolder.h"
#include "WaitingSave.h"

#include <QLockFile>
#include <QRegularExpression>

#include <utility>

FinishGames::FinishGames(Setup setup) : m_setup(std::move(setup)) {}

FinishGames::Outcome FinishGames::run(const QStringList& arguments) const {
    // pkexec always sets PKEXEC_UID, and a system unit starts with the
    // service manager's environment, which never has it.
    if (m_setup.pkexecUid) {
        return {.refusal = Refusal::NotForPkexec, .notes = {}};
    }
    if (m_setup.euid != 0) {
        return {.refusal = Refusal::NotRoot, .notes = {}};
    }
    static const QRegularExpression number(QStringLiteral("^[0-9]{1,10}$"));
    bool isNumber = false;
    const unsigned int uid = arguments.value(1).toUInt(&isNumber);
    if (arguments.size() != 2 || arguments.constFirst() != QStringLiteral("finish-games") ||
        !number.match(arguments.at(1)).hasMatch() || !isNumber) {
        return {.refusal = Refusal::Usage, .notes = {}};
    }

    // Every logout starts this, a Guardian's too, so nothing to do says
    // nothing.
    const std::optional<Account> child = m_setup.lookUpUid(uid);
    if (!child || !child->isCairnChild() ||
        !RootFolder::holds(m_setup.waitingFolder, child->login)) {
        return {};
    }

    // Blocking: a Save running now ends, then this goes on. A lock left by
    // a Save that was killed is taken back, because its process is gone.
    QLockFile lock(m_setup.lock);
    lock.setStaleLockTime(0);
    if (!lock.lock()) {
        return {.refusal = std::nullopt,
                .notes = {QStringLiteral("at %1's logout: the Save lock could not be taken, so "
                                         "nothing was copied")
                              .arg(child->login)}};
    }
    return finish(*child);
}

FinishGames::Outcome FinishGames::finish(const Account& child) const {
    const WaitingSave waiting(m_setup.waitingFolder, m_setup.owner, m_setup.restorecon);
    const WaitingSave::Read read = waiting.read(child);
    // Gone since: a Save that ran meanwhile settled it.
    if (read.found == WaitingSave::Found::Nothing) {
        return {};
    }
    const QString at = QStringLiteral("at %1's logout: ").arg(child.login);
    if (read.found == WaitingSave::Found::Refused) {
        waiting.remove(child.login);
        return {.refusal = std::nullopt,
                .notes = {at + QStringLiteral("the waiting list failed a check, so it was "
                                              "removed and nothing was copied")}};
    }

    const ChildSave::Shared shared{
        .copy = m_setup.copy,
        .file = ChildGamesFile(m_setup.gamesFolder, m_setup.owner, m_setup.restorecon),
        .catalogue = CatalogueRefresh::read(m_setup.gamesFile),
    };
    // Only the lines the window reads back: no copying or waiting.
    QStringList lines;
    const auto keep = [&lines](const QString& line) {
        if (!line.startsWith(QStringLiteral("copying ")) &&
            !line.startsWith(QStringLiteral("waiting "))) {
            lines.append(line);
        }
    };
    const ChildSave::Before before = ChildSave::takeOff(shared, child, read.appIds);
    const ChildSave::Ended ended = ChildSave::run(shared, child, read.appIds, before, keep);
    if (WaitingSave::keeps(ended.held)) {
        return {.refusal = std::nullopt,
                .notes = {at + ended.line + QStringLiteral(", so it waits for the next logout")}};
    }

    Outcome outcome{.refusal = std::nullopt, .notes = {at + ended.line}};
    if (!FinishedSave(m_setup.finishedFolder, m_setup.owner, m_setup.restorecon)
             .write(child.login, lines)) {
        outcome.notes.append(
            at + QStringLiteral("how it ended could not be written down, so it waits for "
                                "the next logout"));
        return outcome;
    }
    if (!waiting.remove(child.login)) {
        outcome.notes.append(at + QStringLiteral("the waiting list could not be removed"));
    }
    return outcome;
}
