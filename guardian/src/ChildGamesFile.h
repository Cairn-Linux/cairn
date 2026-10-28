// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

// One child's own games file, /var/lib/cairn/games/<login>.json (ADR-0041):
// the launcher's games file, version 1, holding only the games the child
// has a copy of and may play. Root writes it and the child's launcher reads
// it, so the folder belongs to root and every account may read both.
class ChildGamesFile {
public:
    // `folder` is where the files go, and `owner` the uid it must belong
    // to: root, and the test's own account in a test. `restorecon` labels a
    // new file for the folder, when the program is there.
    ChildGamesFile(QString folder, unsigned int owner, QString restorecon);

    QString path(const QString& login) const;

    // The child's list. A file that is missing, too big or not a games file
    // is an empty list, which costs only a copy that reports `have`.
    QList<QJsonObject> read(const QString& login) const;

    // Makes the folder when it is missing. False, with nothing written, when
    // the folder is a link, not a folder, not the owner's, or open to others
    // for writing.
    bool write(const QString& login, const QList<QJsonObject>& entries) const;

private:
    bool folderIsReady() const;

    QString m_folder;
    unsigned int m_owner;
    QString m_restorecon;
};
