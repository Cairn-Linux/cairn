// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "Caller.h"
#include "CatalogueRefresh.h"
#include "CopyJob.h"
#include "Refusal.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

// cairn-family save-games: the root side of the Guardian tool's Save on its
// Games page (ADR-0041). It checks the caller and every value, then makes
// the pictures and the full games list again, then takes the games taken
// off out of every child's own games file, then copies each child's newly
// ticked games and writes their file again with them. With no child named
// it only makes the pictures and the full games list again: the window's
// look for new games, which changes no child's list.
//
// It answers in lines of fixed words, checked login names and appids, as it
// goes, and ends with `done` or `refused WORD`. A refusal means nothing
// changed. A child whose games cannot all be given now is a `held` word on
// their own line, and the next child goes ahead.
//
// A child the parent chose to log out now (`--log-out=LOGIN`) is logged
// out by the copy helper before their copy, when they are logged in; any
// other child who is logged in is held, as for "Later".
//
// Before anything changes for a child with games to copy, their whole new
// list is written down as waiting for their next logout. When their copy
// is held for a reason a logout settles, it stays, and the Save says
// `later LOGIN`; cairn-family finish-games then copies it at that logout.
class GamesSave {
public:
    using Say = CopyJob::Say;
    using LookUp = std::function<std::optional<Account>(const QString&)>;

    // Everything the command reads from or does to the machine. main() keeps
    // the defaults and gives the helpers' paths; a test gives stand-ins.
    struct Setup {
        unsigned int euid = Caller::effectiveUid();
        std::optional<QString> pkexecUid = Caller::pkexecUidFromEnvironment();
        LookUp lookUp = lookUpAccount;
        Caller::LookUpUid lookUpUid = lookUpAccountByUid;
        Caller::LookUpHome lookUpHome = ::lookUpHome;
        CatalogueRefresh::Setup catalogue;
        CopyJob::Setup copy;
        // Each child's own games file, and the uid that folder belongs to.
        QString gamesFolder = QStringLiteral("/var/lib/cairn/games");
        unsigned int gamesFolderOwner = 0;
        // The games waiting for each child's next logout, and how the last
        // copy at a logout ended, in folders of the same owner.
        QString waitingFolder = QStringLiteral("/var/lib/cairn/games-waiting");
        QString finishedFolder = QStringLiteral("/var/lib/cairn/games-finished");
        // Held for the whole Save, so two Saves never write one child's
        // list in turn.
        QString lock = QStringLiteral("/run/lock/cairn-save-games");
    };

    struct Outcome {
        std::optional<Refusal> refusal;
        // For the journal, only values already checked: what was asked, and
        // each child's last line, and their `later` line.
        QString asked;
        QStringList children;
    };

    explicit GamesSave(Setup setup);

    Outcome run(const QStringList& arguments, const Say& say) const;

private:
    Setup m_setup;
};
