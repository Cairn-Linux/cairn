// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "PasswordTry.h"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <functional>
#include <optional>

// Reads one entry of `journalctl -o json` into a wrong password or a lock
// (ADR-0054). The trust boundary of the Notices page: everything a child
// can write into the journal must come out as nothing, or as a try on the
// child's own account. No files and no clock, so every rule has a test.
//
// - A try is only an audit USER_AUTH record that failed. Only a program
//   with CAP_AUDIT_WRITE can make one, so a child can cause a real one,
//   by really trying, but cannot write one.
// - A lock is faillock's audit ANOM_LOGIN_FAILURES record, or, for the lock
//   screen, which runs as the person and cannot write audit, faillock's
//   own line from the lock screen's program, by its full path, and only
//   when that program ran as root or as the account it names. A child can
//   fake that line, but only about their own account.
// - A right password is an audit USER_AUTH record of a whole PAM check,
//   never unix_chkpwd's, or a USER_ACCT record, that succeeded, which a
//   child can cause only with the password. A level 1 or 2 child's login
//   at the login screen needs no password, so it is not one.
// - Names in an entry are never shown: each is looked up on this computer,
//   and a name that is no account becomes "no such account".
class JournalTries {
public:
    struct Lookups {
        std::function<std::optional<Account>(const QString&)> byName = lookUpAccount;
        std::function<std::optional<Account>(unsigned int)> byUid = lookUpAccountByUid;
    };

    JournalTries() = default;
    explicit JournalTries(Lookups lookups);

    // One line of journalctl's output as an object; nothing when it is not
    // one.
    static std::optional<QJsonObject> parse(const QByteArray& line);

    std::optional<PasswordTry> tryFrom(const QJsonObject& entry) const;
    std::optional<LockEvent> lockFrom(const QJsonObject& entry) const;
    std::optional<GoodPassword> goodFrom(const QJsonObject& entry) const;

    // The place a program's full path stands for; Other for any other.
    static TryPlace placeOf(const QString& program);
    // A program's file name when it is short and plain; empty otherwise.
    static QString plainName(const QString& program);

private:
    std::optional<Account> accountNamed(const QJsonObject& entry, const QString& field) const;

    Lookups m_lookups;
};
