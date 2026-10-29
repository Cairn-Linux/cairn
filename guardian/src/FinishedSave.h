// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include <optional>

// How the copy at a child's logout ended (ADR-0041):
// /var/lib/cairn/games-finished/<login>. It holds the answer lines
// cairn-family finish-games said for the child, `skipped`, `copied`,
// `have` and the last, `child LOGIN added N removed N [held WORD]`, so
// the Guardian's window reads it as it reads a Save's lines, and dates it
// by the file's time. Only finish-games writes it; any Save that names
// the child removes it, so it shows until the next Save for that child.
class FinishedSave {
public:
    // A thousand games' lines, with room to spare.
    static constexpr qint64 largestFile = qint64{256} * 1024;

    // As WaitingSave's: the folder, the uid it and its files belong to, and
    // the program that labels a new file.
    FinishedSave(QString folder, unsigned int owner, QString restorecon);

    struct Finished {
        QStringList lines;
        // When the copy ended: the file's own time.
        QDateTime when;
    };

    // The child's lines, checked as WaitingSave checks its file, or nothing.
    std::optional<Finished> read(const QString& login) const;

    // False when the file could not be put in place.
    bool write(const QString& login, const QStringList& lines) const;

    // True when the child's file is gone, or was never there.
    bool remove(const QString& login) const;

private:
    QString m_folder;
    unsigned int m_owner;
    QString m_restorecon;
};
