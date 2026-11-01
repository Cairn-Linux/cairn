// SPDX-License-Identifier: Apache-2.0
#include "SaveResults.h"

#include "GamesSaving.h"

#include <QHash>
#include <QVariantList>

#include <algorithm>

namespace {

// The copy's held word, or "" for none, as the window's kind.
GamesSaving::Held heldFor(const QString& word) {
    if (word.isEmpty()) {
        return GamesSaving::Held::NotHeld;
    }
    static const QHash<QString, GamesSaving::Held> held = {
        {QStringLiteral("logged-in"), GamesSaving::Held::LoggedIn},
        {QStringLiteral("busy"), GamesSaving::Held::VisitOpen},
        {QStringLiteral("copying"), GamesSaving::Held::CopyStillRunning},
        {QStringLiteral("logging-out"), GamesSaving::Held::ChildLoggingOut},
        {QStringLiteral("still-running"), GamesSaving::Held::StillRunning},
        {QStringLiteral("stopped"), GamesSaving::Held::Stopped},
        {QStringLiteral("not-set-up"), GamesSaving::Held::NotSetUp},
        {QStringLiteral("no-library"), GamesSaving::Held::NoLibrary},
        {QStringLiteral("no-shared"), GamesSaving::Held::NoShared},
        {QStringLiteral("lists-shared"), GamesSaving::Held::ListsShared},
        {QStringLiteral("unreadable-config"), GamesSaving::Held::UnreadableConfig},
        {QStringLiteral("no-reflink"), GamesSaving::Held::NoReflink},
        {QStringLiteral("failed"), GamesSaving::Held::CopyStopped},
        {QStringLiteral("not-started"), GamesSaving::Held::NotStarted},
        {QStringLiteral("not-written"), GamesSaving::Held::NotWritten},
    };
    return held.value(word, GamesSaving::Held::OtherHold);
}

// A skipped game's reason word as the window's kind.
GamesSaving::Skip skipFor(const QString& word) {
    static const QHash<QString, GamesSaving::Skip> skips = {
        {QStringLiteral("not-shared"), GamesSaving::Skip::NotShared},
        {QStringLiteral("no-catalogue"), GamesSaving::Skip::NoCatalogue},
        {QStringLiteral("unreadable"), GamesSaving::Skip::UnreadableManifest},
        {QStringLiteral("appid"), GamesSaving::Skip::OtherGame},
        {QStringLiteral("installdir"), GamesSaving::Skip::OddFolder},
        {QStringLiteral("beta-branch"), GamesSaving::Skip::BetaBranch},
        {QStringLiteral("not-installed"), GamesSaving::Skip::NotInstalled},
        {QStringLiteral("downloading"), GamesSaving::Skip::Downloading},
        {QStringLiteral("no-folder"), GamesSaving::Skip::NoFolder},
        {QStringLiteral("folder-without-manifest"), GamesSaving::Skip::FolderThere},
        {QStringLiteral("changed"), GamesSaving::Skip::ChangedDuringCopy},
        {QStringLiteral("copy-failed"), GamesSaving::Skip::CopyDidNotWork},
        {QStringLiteral("unreadable-files"), GamesSaving::Skip::UnreadableFiles},
    };
    return skips.value(word, GamesSaving::Skip::OtherSkip);
}

// What the child's copy did with Steam's tools (ADR-0046). A tool they may
// lack says more than the tools they got.
GamesSaving::Tools toolsFor(const SaveReport::Child& child) {
    if (child.noTools) {
        return GamesSaving::Tools::NoToolsToCopy;
    }
    if (child.toolsMissing) {
        return GamesSaving::Tools::ToolsMissing;
    }
    if (child.toolsCopied > 0) {
        return GamesSaving::Tools::ToolsCopied;
    }
    return GamesSaving::Tools::NoToolNews;
}

} // namespace

GamesSaving::Progress SaveResults::progressFor(SaveReport::Stage stage) {
    switch (stage) {
    case SaveReport::Stage::Looking:
        return GamesSaving::Progress::Looking;
    case SaveReport::Stage::SharingTools:
        return GamesSaving::Progress::SharingTools;
    case SaveReport::Stage::Writing:
        return GamesSaving::Progress::Writing;
    case SaveReport::Stage::Copying:
        return GamesSaving::Progress::CopyingGames;
    case SaveReport::Stage::CopyingTools:
        return GamesSaving::Progress::CopyingTools;
    case SaveReport::Stage::Waiting:
        return GamesSaving::Progress::WaitingForLogout;
    case SaveReport::Stage::Starting:
        break;
    }
    return GamesSaving::Progress::Starting;
}

GamesSaving::Reason SaveResults::reasonFor(const QString& answer) {
    static const QHash<QString, GamesSaving::Reason> reasons = {
        {QStringLiteral("not-root"), GamesSaving::Reason::NotRoot},
        {QStringLiteral("not-a-grown-up"), GamesSaving::Reason::NotAGrownUp},
        {QStringLiteral("usage"), GamesSaving::Reason::Usage},
        {QStringLiteral("bad-name"), GamesSaving::Reason::Usage},
        {QStringLiteral("bad-appid"), GamesSaving::Reason::Usage},
        {QStringLiteral("not-a-child"), GamesSaving::Reason::NotAChild},
        {QStringLiteral("busy"), GamesSaving::Reason::AnotherSave},
        {QStringLiteral("dismissed"), GamesSaving::Reason::Dismissed},
        {QStringLiteral("not-authorized"), GamesSaving::Reason::NotAuthorized},
        // SaveRun's own word; cairn-family has no refusal of that name.
        {QStringLiteral("not-started"), GamesSaving::Reason::CommandNotStarted},
    };
    return reasons.value(answer, GamesSaving::Reason::Unknown);
}

QHash<QString, QString> SaveResults::titlesOf(const QList<GameShelf::Game>& games) {
    QHash<QString, QString> titles;
    for (const GameShelf::Game& game : games) {
        titles.insert(game.appId, game.title);
    }
    return titles;
}

QVariantMap SaveResults::forChild(const SaveReport::Child& child, const QString& name,
                                  const QHash<QString, QString>& titles) {
    QVariantList skipped;
    for (const SaveReport::Skip& skip : child.skipped) {
        skipped.append(QVariantMap{
            {QStringLiteral("appId"), skip.appId},
            {QStringLiteral("title"), titles.value(skip.appId)},
            {QStringLiteral("reason"), static_cast<int>(skipFor(skip.reason))},
        });
    }
    return QVariantMap{
        {QStringLiteral("login"), child.login},
        {QStringLiteral("name"), name},
        {QStringLiteral("added"), child.added.value_or(-1)},
        {QStringLiteral("removed"), child.removed.value_or(-1)},
        {QStringLiteral("held"), static_cast<int>(heldFor(child.held.value_or(QString())))},
        {QStringLiteral("skipped"), skipped},
        {QStringLiteral("tools"), static_cast<int>(toolsFor(child))},
    };
}

bool SaveResults::allGiven(const QList<SaveReport::Child>& children) {
    return std::ranges::none_of(children, [](const SaveReport::Child& child) {
        return child.held.has_value() || !child.skipped.isEmpty();
    });
}
