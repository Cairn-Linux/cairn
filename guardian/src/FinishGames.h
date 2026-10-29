// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "Caller.h"
#include "CopyJob.h"
#include "Refusal.h"

#include <QString>
#include <QStringList>

#include <optional>

// cairn-family finish-games UID: copies the games a Save left waiting for
// a child, once that child has logged out (ADR-0041). systemd starts it
// when the child's last session has ended, through the unit
// cairn-finish-games@UID.service; it has no polkit action, and refuses to
// run under pkexec.
//
// It takes the Save lock, waiting for a Save that is running, then reads
// the child's waiting list again: a Save that ran meanwhile has settled it.
// The copy is a Save's for one child, from the games file as it is, with
// no pictures made: the games not on the list go, the new ones are copied
// as the child, and the child's list is written. A copy held for a reason
// a logout settles keeps waiting for the next logout. Any other ending
// writes the child's lines down for the Guardian's window, and the wait
// is over.
class FinishGames {
public:
    struct Setup {
        unsigned int euid = Caller::effectiveUid();
        std::optional<QString> pkexecUid = Caller::pkexecUidFromEnvironment();
        Caller::LookUpUid lookUpUid = lookUpAccountByUid;
        CopyJob::Setup copy;
        QString gamesFile = QStringLiteral("/var/lib/cairn/games.json");
        QString restorecon = QStringLiteral("/usr/sbin/restorecon");
        // The same folders and lock as a Save's, and the uid the folders
        // belong to.
        QString gamesFolder = QStringLiteral("/var/lib/cairn/games");
        QString waitingFolder = QStringLiteral("/var/lib/cairn/games-waiting");
        QString finishedFolder = QStringLiteral("/var/lib/cairn/games-finished");
        unsigned int owner = 0;
        QString lock = QStringLiteral("/run/lock/cairn-save-games");
    };

    struct Outcome {
        std::optional<Refusal> refusal;
        // For the journal: nothing when there was nothing to do.
        QStringList notes;
    };

    explicit FinishGames(Setup setup);

    // `arguments` starts with "finish-games".
    Outcome run(const QStringList& arguments) const;

private:
    Outcome finish(const Account& child) const;

    Setup m_setup;
};
