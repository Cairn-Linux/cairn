// SPDX-License-Identifier: Apache-2.0
#include "FailedLogin.h"

#include <QFile>

FailedLogin::FailedLogin(QObject* parent) : QObject(parent) {}

QUrl FailedLogin::visitsFile() const {
    return m_visitsFile;
}

void FailedLogin::setVisitsFile(const QUrl& file) {
    if (file == m_visitsFile) {
        return;
    }
    m_visitsFile = file;
    emit visitsFileChanged();
}

AccountLock* FailedLogin::accountLock() const {
    return m_accountLock;
}

void FailedLogin::setAccountLock(AccountLock* lock) {
    if (lock == m_accountLock) {
        return;
    }
    m_accountLock = lock;
    emit accountLockChanged();
}

// A lock comes first: faillock is in PAM's auth step and the visit list in
// its account step, so a locked account is refused before the list is read,
// even with the right password. Then the list: PAM refuses a listed child
// whatever they type, and SDDM does not say which step said no, so a listed
// child is never told their password was wrong. Only a tile that asks for a
// password is ever locked: L1 and L2 children are let in before faillock.
FailedLogin::Reason FailedLogin::reasonFor(const QString& loginName, bool gavePassword) {
    AccountLock::State lock = AccountLock::State::Unknown;
    if (gavePassword && m_accountLock && m_accountLock->loginName() == loginName) {
        m_accountLock->refresh();
        if (m_accountLock->locked()) {
            return Reason::Locked;
        }
        lock = m_accountLock->state();
    }
    if (lists(m_visitsFile.toLocalFile(), loginName)) {
        return Reason::SteamVisit;
    }
    if (!gavePassword) {
        return Reason::NeedsGrownUp;
    }
    switch (lock) {
    case AccountLock::State::Open:
    case AccountLock::State::OpenAgain:
        return Reason::TryAgain;
    case AccountLock::State::OneTryLeft:
        return Reason::OneTryLeft;
    case AccountLock::State::OneTryLeftNoTime:
        return Reason::OneTryLeftNoTime;
    default:
        return Reason::WrongPassword;
    }
}

bool FailedLogin::lists(const QString& file, const QString& loginName) {
    QFile list(file);
    if (loginName.isEmpty() || !list.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    while (!list.atEnd()) {
        if (QString::fromUtf8(list.readLine()).trimmed() == loginName) {
            return true;
        }
    }
    return false;
}
