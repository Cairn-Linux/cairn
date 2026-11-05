// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "AccountLock.h"

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

#include <cstdint>

// Why a login the screen asked for failed, so the screen can say so plainly.
//
// PAM never says why it refused. The screen can know two reasons. A grown-up
// may have the child's Steam open from their own login (ADR-0032): PAM
// refuses a child named in the list of children on a visit, and the screen
// reads the same list. Or faillock has locked the account, or will at the
// next wrong try, which AccountLock reads from faillock's own record
// (ADR-0052). PAM stays the gate; this only picks the words.
class FailedLogin : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // The list the root helper writes and PAM reads. Tests give another.
    Q_PROPERTY(QUrl visitsFile READ visitsFile WRITE setVisitsFile NOTIFY visitsFileChanged)
    // The lock of the account whose password row is open. The screen owns
    // it; this only looks at it.
    Q_PROPERTY(
        AccountLock* accountLock READ accountLock WRITE setAccountLock NOTIFY accountLockChanged)

public:
    enum class Reason : std::uint8_t {
        Locked,           // faillock has locked the account; the password may be right
        SteamVisit,       // a grown-up has this child's Steam open
        WrongPassword,    // a password was not taken, and the lock is unknown
        TryAgain,         // a password was not taken; two or more tries are left
        OneTryLeft,       // a password was not taken; the next wrong one locks
        OneTryLeftNoTime, // the same, for a time not said in minutes
        NeedsGrownUp,     // anything else
    };
    Q_ENUM(Reason)

    explicit FailedLogin(QObject* parent = nullptr);

    QUrl visitsFile() const;
    void setVisitsFile(const QUrl& file);

    AccountLock* accountLock() const;
    void setAccountLock(AccountLock* lock);

    // Reads the lock again first: SDDM says a login failed only after PAM
    // has written the try.
    Q_INVOKABLE Reason reasonFor(const QString& loginName, bool gavePassword);

    // Whether the list names the account, one name to a line, as
    // pam_listfile reads it. A list that cannot be read names no one.
    static bool lists(const QString& file, const QString& loginName);

signals:
    void visitsFileChanged();
    void accountLockChanged();

private:
    QUrl m_visitsFile = QUrl::fromLocalFile(QStringLiteral("/run/cairn/steam-visits"));
    // Not owned: QPointer becomes null if the screen's lock goes away.
    QPointer<AccountLock> m_accountLock;
};
