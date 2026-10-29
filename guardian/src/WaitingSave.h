// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"

#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

// The games a Save could not copy yet because the child was logged in,
// kept until the child next logs out (ADR-0041):
// /var/lib/cairn/games-waiting/<login>. It is one line, exactly that
// child's argument to the Save, such as `ada=570,620`: their whole new
// list, read back with the Save's own rule. Root writes it, mode 0644;
// cairn-family finish-games reads it at the child's logout, and the
// Guardian's window reads it to show the games as waiting.
class WaitingSave {
public:
    // A thousand appids and a login fit in under 12 KiB.
    static constexpr qint64 largestFile = qint64{16} * 1024;

    // `folder` is where the files go, and `owner` the uid it and every file
    // in it must belong to: root, and the test's own account in a test.
    // `restorecon` labels a new file for the folder, when it is there.
    WaitingSave(QString folder, unsigned int owner, QString restorecon);

    enum class Found : std::uint8_t {
        // No file: nothing waits for the child.
        Nothing,
        Waiting,
        // A file is there but failed a check, so it must never be copied.
        Refused,
    };

    struct Read {
        Found found = Found::Nothing;
        // The child's whole new list, when Waiting; never empty then.
        QStringList appIds;
    };

    // The child's file, checked: the folder is root's own, the file is a
    // regular file of root's under the size cap, never followed through a
    // link, its one line is a list the Save would take, it names this
    // child, and the account is a child Cairn made.
    Read read(const Account& child) const;

    // Writes the child's whole new list. False when it could not be put in
    // place; the old file, if any, is then as it was.
    bool write(const QString& login, const QStringList& appIds) const;

    // True when the child's file is gone, or was never there.
    bool remove(const QString& login) const;

    // Whether a child whose copy ended with this held word keeps waiting:
    // only for the words a logout settles. Every other ending, a clean one
    // included, ends the wait.
    static bool keeps(const std::optional<QString>& held);

private:
    QString m_folder;
    unsigned int m_owner;
    QString m_restorecon;
};
