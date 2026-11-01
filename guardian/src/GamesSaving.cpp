// SPDX-License-Identifier: Apache-2.0
#include "GamesSaving.h"

#include "GamesRequest.h"
#include "SaveResults.h"
#include "SaveRun.h"

#include <QLocale>
#include <QVariantMap>

#include <utility>

GamesSaving::GamesSaving(const QString& pkexec, const QString& command, QString catalogue,
                         QObject* parent)
    : QObject(parent), m_run(new SaveRun(pkexec, command, this)),
      m_catalogue(std::move(catalogue)) {
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
    return SaveResults::progressFor(m_report.stage());
}

QString GamesSaving::progressName() const {
    return m_report.current().isEmpty() ? QString() : nameOf(m_report.current());
}

QVariantList GamesSaving::copiedSoFar() const {
    QVariantList games;
    for (const SaveReport::Copied& game : m_report.copied()) {
        games.append(QVariantMap{
            {QStringLiteral("login"), game.login},
            {QStringLiteral("name"), nameOf(game.login)},
            {QStringLiteral("appId"), game.appId},
            {QStringLiteral("title"), m_titles.value(game.appId)},
            {QStringLiteral("had"), game.had},
        });
    }
    return games;
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

bool GamesSaving::picturesKept() const {
    return m_report.picturesKept().value_or(false);
}

QString GamesSaving::picturesMissing() const {
    return titlesInWords(m_report.picturesMissing());
}

QString GamesSaving::picturesRefused() const {
    return titlesInWords(m_report.picturesRefused());
}

// Only about a Save that gave some child something: a look gives no child
// anything, and neither does a Save whose copies were all held.
bool GamesSaving::toolsLeftOut() const {
    return m_kind == Kind::Save && m_report.toolsLeftOut() && m_report.copiedForSomeone();
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

void GamesSaving::close() {
    clearOutcome();
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

// The first line means the password was accepted and the command runs:
// cairn-family says "looking" before its first slow step.
void GamesSaving::lineRead(const QString& line) {
    m_report.take(line);
    m_step = Step::Saving;
    emit changed();
}

// The full list is read again for the titles, so a game the command has
// just found is named by its title too.
void GamesSaving::finished(const QString& answer) {
    m_titles.insert(SaveResults::titlesOf(GameShelf::read(m_catalogue, {}).games()));
    m_step = answer == QLatin1String("done") ? Step::Saved : Step::NotSaved;
    m_reason = m_step == Step::Saved ? Reason::NoReason : reasonFor(answer);
    emit changed();
    emit ended();
}

QString GamesSaving::nameOf(const QString& login) const {
    return m_names.value(login, login);
}

// A game neither the page nor the full list names, which only a change to
// the list since the command read it could leave, is named by its number.
QString GamesSaving::titlesInWords(const QStringList& appIds) const {
    QStringList titles;
    for (const QString& appId : appIds) {
        const QString title = m_titles.value(appId);
        titles.append(title.isEmpty() ? tr("Game %1").arg(appId) : title);
    }
    return QLocale().createSeparatedList(titles);
}
