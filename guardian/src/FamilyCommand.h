// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "Caller.h"
#include "FamilyRequest.h"
#include "PasswordLock.h"
#include "Refusal.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

// cairn-family: the root command that makes a child's account and changes
// a child's level (ADR-0039). A Guardian's window runs it through pkexec;
// the family's setup runs it as root with sudo. Either way it checks the
// caller and every value itself, and changes only accounts Cairn made.
class FamilyCommand {
public:
    using LookUp = std::function<std::optional<Account>(const QString&)>;
    using LookUpUid = std::function<std::optional<Account>(unsigned int)>;
    using PasswordLocked = std::function<std::optional<bool>(const QString&)>;

    // Everything the command reads from or does to the machine. main() keeps
    // the defaults, the real ones; a test replaces each with a stand-in.
    struct Setup {
        // The system's tools, by absolute path.
        QString useradd = QStringLiteral("/usr/sbin/useradd");
        QString passwd = QStringLiteral("/usr/bin/passwd");
        QString gpasswd = QStringLiteral("/usr/bin/gpasswd");
        QString usermod = QStringLiteral("/usr/sbin/usermod");
        QString restorecon = QStringLiteral("/usr/sbin/restorecon");
        // The folder for "Steam for <child>" entries, and the window they
        // name; main() gives where this build puts them.
        QString applications;
        QString steamFor;
        // Each child's games file (ADR-0041), in a folder that belongs to
        // `gamesOwner`: a new child never inherits an old account's of the
        // same name.
        QString gamesFolder = QStringLiteral("/var/lib/cairn/games");
        unsigned int gamesOwner = 0;
        unsigned int euid = Caller::effectiveUid();
        // PKEXEC_UID, when pkexec started the command.
        std::optional<QString> pkexecUid = Caller::pkexecUidFromEnvironment();
        LookUp lookUp = lookUpAccount;
        LookUpUid lookUpUid = lookUpAccountByUid;
        PasswordLocked passwordLocked = PasswordLock::isLocked;
    };

    struct Outcome {
        // Nothing when the change was made, or was already in place.
        std::optional<Refusal> refusal;
        // What was asked, for the journal: only values already checked.
        QString asked;
    };

    explicit FamilyCommand(Setup setup);

    Outcome run(const QStringList& arguments) const;

private:
    std::optional<Refusal> makeChild(const FamilyRequest& request) const;
    bool removeOldGames(const QString& login) const;
    std::optional<Refusal> changeChild(const Account& child, const FamilyRequest& request) const;
    std::optional<Refusal> setLevel(const Account& child, int level) const;
    std::optional<Refusal> writeEntry(const QString& login, const QString& name) const;

    Setup m_setup;
};
