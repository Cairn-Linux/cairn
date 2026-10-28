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
// ticked games and writes their file again with them.
//
// It answers in lines of fixed words, checked login names and appids, as it
// goes, and ends with `done` or `refused WORD`. A refusal means nothing
// changed. A child whose games cannot all be given now is a `held` word on
// their own line, and the next child goes ahead.
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
        // Held for the whole Save, so two Saves never write one child's
        // list in turn.
        QString lock = QStringLiteral("/run/lock/cairn-save-games");
    };

    struct Outcome {
        std::optional<Refusal> refusal;
        // For the journal, only values already checked: what was asked, and
        // each child's last line.
        QString asked;
        QStringList children;
    };

    explicit GamesSave(Setup setup);

    Outcome run(const QStringList& arguments, const Say& say) const;

private:
    Setup m_setup;
};
