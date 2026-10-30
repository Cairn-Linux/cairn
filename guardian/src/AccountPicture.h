// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QString>

class QDBusPendingCallWatcher;

// A grown-up's own picture, the one the login screen and Plasma show,
// through AccountsService on the system bus (ADR-0043). It is what
// Plasma's Users page uses too. polkit lets an account change its own
// picture without a password; AccountsService copies the file into
// /var/lib/AccountsService/icons, so the file may go afterwards.
class AccountPicture : public QObject {
    Q_OBJECT

public:
    AccountPicture(QDBusConnection bus, unsigned int uid, QObject* parent = nullptr);

    // The full path of the picture AccountsService has for the account now,
    // or "" when it has none, its file is not there, or it cannot be asked.
    QString current() const;

    bool running() const;
    // Asks AccountsService to use the picture in `file`. False, and nothing
    // is asked, while an earlier one is still running or when the account
    // cannot be found.
    bool set(const QString& file);

signals:
    // Whether AccountsService took the picture.
    void finished(bool changed);

private:
    QString userPath() const;
    void answered(QDBusPendingCallWatcher* watcher);

    QDBusConnection m_bus;
    unsigned int m_uid;
    QDBusPendingCallWatcher* m_call = nullptr;
};
