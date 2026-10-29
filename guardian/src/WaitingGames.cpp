// SPDX-License-Identifier: Apache-2.0
#include "WaitingGames.h"

#include "GameShelf.h"
#include "SaveReport.h"
#include "SaveResults.h"
#include "WaitingSave.h"

#include <QFileInfo>
#include <QFileSystemWatcher>

#include <utility>

WaitingGames::WaitingGames(Folders folders, QObject* parent)
    : QObject(parent), m_folders(std::move(folders)), m_watcher(new QFileSystemWatcher(this)) {
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &WaitingGames::changed);
    watch();
}

// A login goes into a file name only when it follows cairn-family's rule,
// which GameShelf::childListPath checks; any other has no files.
void WaitingGames::read(const std::vector<Account>& accounts) {
    watch();
    const WaitingSave waiting(m_folders.waiting, m_folders.owner, QString());
    const FinishedSave finished(m_folders.finished, m_folders.owner, QString());
    m_files.clear();
    for (const Account& account : accounts) {
        const QString list = GameShelf::childListPath(m_folders.childLists, account.login);
        if (!account.isCairnChild() || list.isEmpty()) {
            continue;
        }
        const WaitingSave::Read read = waiting.read(account);
        Files files{.saved = GameShelf::listedIds(list),
                    .waiting = {},
                    .finished = {},
                    .found = read.found};
        if (read.found == WaitingSave::Found::Waiting) {
            files.waiting = read.appIds;
        }
        files.finished = finished.read(account.login);
        m_files.insert(account.login, files);
    }
}

QStringList WaitingGames::baseline(const QString& login) const {
    const Files files = m_files.value(login);
    return files.waiting.value_or(files.saved);
}

bool WaitingGames::isWaiting(const QString& login) const {
    return m_files.value(login).waiting.has_value();
}

bool WaitingGames::waitEnded(const QString& login) const {
    return m_files.contains(login) && m_files.value(login).found == WaitingSave::Found::Nothing;
}

QStringList WaitingGames::saved(const QString& login) const {
    return m_files.value(login).saved;
}

QVariantList WaitingGames::marked(const QVariantList& games, const QString& login) const {
    const Files files = m_files.value(login);
    QVariantList marked;
    for (const QVariant& value : games) {
        QVariantMap game = value.toMap();
        const QString appId = game.value(QStringLiteral("appId")).toString();
        const bool waits =
            files.waiting && files.waiting->contains(appId) && !files.saved.contains(appId);
        game.insert(QStringLiteral("saved"), baseline(login).contains(appId));
        game.insert(QStringLiteral("waiting"), waits);
        marked.append(game);
    }
    return marked;
}

QVariantMap WaitingGames::finished(const QString& login, const QString& name,
                                   const QHash<QString, QString>& titles) const {
    const std::optional<FinishedSave::Finished> finished = m_files.value(login).finished;
    if (!finished) {
        return {};
    }
    SaveReport report({login});
    for (const QString& line : finished->lines) {
        report.take(line);
    }
    const QList<SaveReport::Child> children = report.children();
    if (children.isEmpty() || !children.constFirst().added) {
        return {};
    }
    return QVariantMap{
        {QStringLiteral("when"), finished->when},
        {QStringLiteral("clean"), SaveResults::allGiven(children)},
        {QStringLiteral("result"), SaveResults::forChild(children.constFirst(), name, titles)},
    };
}

// A folder made after the window opened is watched from the next reading.
void WaitingGames::watch() {
    const QStringList folders{m_folders.waiting, m_folders.finished};
    const QStringList watched = m_watcher->directories();
    for (const QString& folder : folders) {
        if (!watched.contains(folder) && QFileInfo::exists(folder)) {
            m_watcher->addPath(folder);
        }
    }
}
