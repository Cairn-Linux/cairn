// SPDX-License-Identifier: Apache-2.0
#include "GamesSaving.h"

#include "GamesRequest.h"
#include "SaveResults.h"
#include "SaveRun.h"

#include <QLocale>
#include <QVariantMap>

#include <optional>
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

QString GamesSaving::newGames() const {
    return lookMadeTheList() ? titlesInWords(m_newGames) : QString();
}

bool GamesSaving::noNewGames() const {
    return lookMadeTheList() && m_newGames.isEmpty();
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
    m_shown = games;
    m_report = SaveReport(request.logins());
    if (request.refusal) {
        m_step = Step::NotSaved;
        m_reason = SaveResults::reasonFor(refusalWord(*request.refusal));
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

// The first line means the password was accepted and the command runs:
// cairn-family says "looking" before its first slow step.
void GamesSaving::lineRead(const QString& line) {
    m_report.take(line);
    m_step = Step::Saving;
    emit changed();
}

// The full list is read again for the titles, so a game the command has
// just found is named by its title too, and for the games a look found.
void GamesSaving::finished(const QString& answer) {
    const QList<GameShelf::Game> listed = GameShelf::read(m_catalogue, {}).games();
    m_titles.insert(SaveResults::titlesOf(listed));
    m_newGames = SaveResults::newAppIds(listed, m_shown);
    m_step = answer == QLatin1String("done") ? Step::Saved : Step::NotSaved;
    m_reason = m_step == Step::Saved ? Reason::NoReason : SaveResults::reasonFor(answer);
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

// Only a look that went to its end, and said it made the full games list
// again, knows which games are new. A Save is about the children's lists,
// and a list kept from before holds nothing the look found.
bool GamesSaving::lookMadeTheList() const {
    const std::optional<bool> kept = m_report.catalogueKept();
    return m_kind == Kind::Look && m_step == Step::Saved && kept.has_value() && !*kept;
}
