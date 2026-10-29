// SPDX-License-Identifier: Apache-2.0
#include "GamesSaving.h"

#include "GamesRequest.h"
#include "SaveResults.h"
#include "SaveRun.h"

GamesSaving::GamesSaving(const QString& pkexec, const QString& command, QObject* parent)
    : QObject(parent), m_run(new SaveRun(pkexec, command, this)) {
    connect(m_run, &SaveRun::lineRead, this, &GamesSaving::lineRead);
    connect(m_run, &SaveRun::finished, this, &GamesSaving::finished);
}

GamesSaving::Step GamesSaving::step() const {
    return m_step;
}

GamesSaving::Kind GamesSaving::kind() const {
    return m_kind;
}

GamesSaving::Reason GamesSaving::reason() const {
    return m_reason;
}

GamesSaving::Progress GamesSaving::progress() const {
    switch (m_report.stage()) {
    case SaveReport::Stage::Looking:
        return Progress::Looking;
    case SaveReport::Stage::Writing:
        return Progress::Writing;
    case SaveReport::Stage::Copying:
        return Progress::CopyingGames;
    case SaveReport::Stage::Waiting:
        return Progress::WaitingForLogout;
    case SaveReport::Stage::LoggingOut:
        return Progress::LoggingOut;
    case SaveReport::Stage::Starting:
        break;
    }
    return Progress::Starting;
}

QString GamesSaving::progressName() const {
    return m_report.current().isEmpty() ? QString() : nameOf(m_report.current());
}

QVariantList GamesSaving::results() const {
    QVariantList results;
    for (const SaveReport::Child& child : m_report.children()) {
        results.append(SaveResults::forChild(child, nameOf(child.login), m_titles));
    }
    return results;
}

bool GamesSaving::allGiven() const {
    return SaveResults::allGiven(m_report.children());
}

bool GamesSaving::catalogueKept() const {
    return m_report.catalogueKept().value_or(false);
}

bool GamesSaving::busy() const {
    return m_step == Step::Asking || m_step == Step::Saving;
}

// The window checks the arguments with cairn-family's own rule before it
// asks for a password, so arguments the command would refuse are refused
// here without one.
bool GamesSaving::start(Kind kind, const QStringList& arguments,
                        const std::vector<ListedChild>& children,
                        const QList<GameShelf::Game>& games) {
    if (busy()) {
        return false;
    }
    const GamesRequest request = GamesRequest::fromArguments(arguments);
    m_kind = kind;
    m_names.clear();
    for (const ListedChild& child : children) {
        m_names.insert(child.login, child.name);
    }
    m_titles = SaveResults::titlesOf(games);
    m_report = SaveReport(request.logins());
    if (request.refusal) {
        m_step = Step::NotSaved;
        m_reason = reasonFor(refusalWord(*request.refusal));
        emit changed();
        emit ended();
        return true;
    }
    // Asking comes first: when the process cannot start at all, its answer
    // arrives inside start(), and must not be overwritten.
    m_step = Step::Asking;
    m_reason = Reason::NoReason;
    emit changed();
    if (!m_run->start(arguments)) {
        m_step = Step::Ready;
        emit changed();
        return false;
    }
    return true;
}

void GamesSaving::clearOutcome() {
    if (m_step != Step::Saved && m_step != Step::NotSaved) {
        return;
    }
    m_step = Step::Ready;
    m_reason = Reason::NoReason;
    m_report = SaveReport();
    emit changed();
}

QStringList GamesSaving::laterLogins() const {
    QStringList logins;
    for (const SaveReport::Child& child : m_report.children()) {
        if (child.later) {
            logins.append(child.login);
        }
    }
    return logins;
}

GamesSaving::Reason GamesSaving::reasonFor(const QString& answer) {
    static const QHash<QString, Reason> reasons = {
        {QStringLiteral("not-root"), Reason::NotRoot},
        {QStringLiteral("not-a-grown-up"), Reason::NotAGrownUp},
        {QStringLiteral("usage"), Reason::Usage},
        {QStringLiteral("bad-name"), Reason::Usage},
        {QStringLiteral("bad-appid"), Reason::Usage},
        {QStringLiteral("not-a-child"), Reason::NotAChild},
        {QStringLiteral("busy"), Reason::AnotherSave},
        {QStringLiteral("dismissed"), Reason::Dismissed},
        {QStringLiteral("not-authorized"), Reason::NotAuthorized},
        // SaveRun's own word; cairn-family has no refusal of that name.
        {QStringLiteral("not-started"), Reason::CommandNotStarted},
    };
    return reasons.value(answer, Reason::Unknown);
}

GamesSaving::Held GamesSaving::heldFor(const QString& word) {
    if (word.isEmpty()) {
        return Held::NotHeld;
    }
    static const QHash<QString, Held> held = {
        {QStringLiteral("logged-in"), Held::LoggedIn},
        {QStringLiteral("busy"), Held::VisitOpen},
        {QStringLiteral("copying"), Held::CopyStillRunning},
        {QStringLiteral("still-running"), Held::StillRunning},
        {QStringLiteral("stopped"), Held::Stopped},
        {QStringLiteral("not-set-up"), Held::NotSetUp},
        {QStringLiteral("no-library"), Held::NoLibrary},
        {QStringLiteral("no-shared"), Held::NoShared},
        {QStringLiteral("lists-shared"), Held::ListsShared},
        {QStringLiteral("unreadable-config"), Held::UnreadableConfig},
        {QStringLiteral("no-reflink"), Held::NoReflink},
        {QStringLiteral("failed"), Held::CopyStopped},
        {QStringLiteral("not-started"), Held::NotStarted},
        {QStringLiteral("not-written"), Held::NotWritten},
        {QStringLiteral("stuck"), Held::Stuck},
    };
    return held.value(word, Held::OtherHold);
}

GamesSaving::Skip GamesSaving::skipFor(const QString& word) {
    static const QHash<QString, Skip> skips = {
        {QStringLiteral("not-shared"), Skip::NotShared},
        {QStringLiteral("no-catalogue"), Skip::NoCatalogue},
        {QStringLiteral("unreadable"), Skip::UnreadableManifest},
        {QStringLiteral("appid"), Skip::OtherGame},
        {QStringLiteral("installdir"), Skip::OddFolder},
        {QStringLiteral("beta-branch"), Skip::BetaBranch},
        {QStringLiteral("not-installed"), Skip::NotInstalled},
        {QStringLiteral("downloading"), Skip::Downloading},
        {QStringLiteral("no-folder"), Skip::NoFolder},
        {QStringLiteral("folder-without-manifest"), Skip::FolderThere},
        {QStringLiteral("changed"), Skip::ChangedDuringCopy},
        {QStringLiteral("copy-failed"), Skip::CopyDidNotWork},
        {QStringLiteral("unreadable-files"), Skip::UnreadableFiles},
    };
    return skips.value(word, Skip::OtherSkip);
}

// The first line means the password was accepted and the command runs:
// cairn-family says "looking" before its first slow step.
void GamesSaving::lineRead(const QString& line) {
    m_report.take(line);
    m_step = Step::Saving;
    emit changed();
}

void GamesSaving::finished(const QString& answer) {
    m_step = answer == QLatin1String("done") ? Step::Saved : Step::NotSaved;
    m_reason = m_step == Step::Saved ? Reason::NoReason : reasonFor(answer);
    emit changed();
    emit ended();
}

QString GamesSaving::nameOf(const QString& login) const {
    return m_names.value(login, login);
}
