// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "FinishedSave.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <optional>
#include <vector>

class QFileSystemWatcher;

// The Games page's view of each child's three files (ADR-0041): their own
// games list as saved, the games a Save left waiting for their next logout,
// and how the copy at their last logout ended. The window reads them
// without root, with the same checks root makes, and watches the waiting
// and finished folders, so an open page follows a copy at a logout.
//
// A child's waiting list is the page's starting point for them: what the
// parent last saved, even while some of it is not copied yet.
class WaitingGames : public QObject {
    Q_OBJECT

public:
    // The three folders, and the uid they and their files belong to: root,
    // and the test's own account in a test.
    struct Folders {
        QString childLists = QStringLiteral("/var/lib/cairn/games");
        QString waiting = QStringLiteral("/var/lib/cairn/games-waiting");
        QString finished = QStringLiteral("/var/lib/cairn/games-finished");
        unsigned int owner = 0;
    };

    explicit WaitingGames(Folders folders, QObject* parent = nullptr);

    // Reads every Cairn child's files among these accounts again.
    void read(const std::vector<Account>& accounts);

    // The child's list as the parent last saved it: their waiting list, or
    // the list their file holds.
    QStringList baseline(const QString& login) const;
    bool isWaiting(const QString& login) const;
    // The list the child's own file holds now: the games they have.
    QStringList saved(const QString& login) const;

    // The page's games, as GameShelf gives them, for the child: saved is
    // true for a game on the baseline, and waiting for one that waits for
    // the child's next logout.
    QVariantList marked(const QVariantList& games, const QString& login) const;

    // The copy at the child's last logout as a map: when, a date and time,
    // clean, true when every game was given, and result, as SaveResults
    // gives it. Empty when there is none to show. `titles` names the games
    // left out.
    QVariantMap finished(const QString& login, const QString& name,
                         const QHash<QString, QString>& titles) const;

signals:
    // A file in the waiting or finished folder came, went or changed.
    void changed();

private:
    struct Files {
        QStringList saved;
        std::optional<QStringList> waiting;
        std::optional<FinishedSave::Finished> finished;
    };

    void watch();

    Folders m_folders;
    QFileSystemWatcher* m_watcher;
    QHash<QString, Files> m_files;
};
