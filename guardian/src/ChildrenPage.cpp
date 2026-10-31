// SPDX-License-Identifier: Apache-2.0
#include "ChildrenPage.h"

#include "FamilyChange.h"
#include "FamilyRequest.h"
#include "LoginName.h"

#include <QHash>

#include <algorithm>
#include <utility>

ChildrenPage::ChildrenPage(Setup setup, QObject* parent)
    : QObject(parent), m_setup(std::move(setup)),
      m_change(new FamilyChange(m_setup.pkexec, m_setup.command, this)) {
    connect(m_change, &FamilyChange::finished, this, &ChildrenPage::finished);
    reload();
}

QVariantList ChildrenPage::listedChildren() const {
    return loginsNamesAndLevels(m_children);
}

ChildrenPage::Step ChildrenPage::step() const {
    return m_step;
}

ChildrenPage::Reason ChildrenPage::reason() const {
    return m_reason;
}

QString ChildrenPage::subject() const {
    return m_subject;
}

int ChildrenPage::level() const {
    return m_level;
}

bool ChildrenPage::busy() const {
    return m_step == Step::Asking;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool ChildrenPage::needsShortName(const QString& name) const {
    return !name.trimmed().isEmpty() && !loginFromName(name.trimmed()).has_value();
}

// The login name comes from the name, or from the short name when the name
// gives none, and is never one an account or group already has: given an
// existing child's login, create-child would change that child instead.
void ChildrenPage::addChild(const QString& name, const QString& shortName, int level) {
    if (busy()) {
        return;
    }
    const QString fullName = name.trimmed();
    m_subject = fullName;
    m_level = level;
    if (fullName.isEmpty()) {
        setStep(Step::NotChanged, Reason::NoName);
        return;
    }
    std::optional<QString> login = loginFromName(fullName);
    if (!login) {
        login = loginFromName(shortName.trimmed());
    }
    if (!login) {
        setStep(Step::NotChanged, Reason::NoShortName);
        return;
    }
    const std::optional<QString> free = loginToAdd(*login, fullName);
    if (!free) {
        setStep(Step::NotChanged, Reason::Taken);
        return;
    }
    m_adding = UnfinishedAdd{.login = *free, .fullName = fullName};
    run({QStringLiteral("create-child"), *free, QString::number(level),
         QStringLiteral("--full-name"), fullName},
        Step::Added);
}

// After an add that may have stopped part way, adding the same name again
// gives create-child the same login, so the second run finishes the first
// (ADR-0039) instead of making a second account with that name. It does
// only while that login is free or is still that very child.
std::optional<QString> ChildrenPage::loginToAdd(const QString& login,
                                                const QString& fullName) const {
    if (m_unfinished && m_unfinished->fullName == fullName) {
        const QString& earlier = m_unfinished->login;
        const bool sameChild = std::ranges::any_of(m_children, [&](const ListedChild& child) {
            return child.login == earlier && child.name == fullName;
        });
        if (sameChild || !m_setup.nameInUse(earlier)) {
            return earlier;
        }
    }
    return freeLogin(login, m_setup.nameInUse);
}

// Only a child the page lists, so the page never names another account.
void ChildrenPage::changeLevel(const QString& login, int level) {
    if (busy()) {
        return;
    }
    const auto child = std::ranges::find(m_children, login, &ListedChild::login);
    m_subject = child == m_children.end() ? login : child->name;
    m_level = level;
    if (child == m_children.end()) {
        setStep(Step::NotChanged, Reason::NotAChild);
        return;
    }
    run({QStringLiteral("set-level"), login, QString::number(level)}, Step::LevelChanged);
}

void ChildrenPage::reload() {
    m_children = childrenAmong(m_setup.accounts());
    emit listedChildrenChanged();
}

ChildrenPage::Reason ChildrenPage::reasonFor(const QString& answer) {
    static const QHash<QString, Reason> reasons = {
        {QStringLiteral("not-root"), Reason::NotRoot},
        {QStringLiteral("not-a-grown-up"), Reason::NotAGrownUp},
        {QStringLiteral("usage"), Reason::Usage},
        {QStringLiteral("bad-name"), Reason::BadName},
        {QStringLiteral("bad-full-name"), Reason::BadFullName},
        {QStringLiteral("bad-level"), Reason::BadLevel},
        {QStringLiteral("taken"), Reason::Taken},
        {QStringLiteral("not-a-child"), Reason::NotAChild},
        {QStringLiteral("failed"), Reason::Failed},
        {QStringLiteral("dismissed"), Reason::Dismissed},
        {QStringLiteral("not-authorized"), Reason::NotAuthorized},
    };
    return reasons.value(answer, Reason::Unknown);
}

// The window checks every value with cairn-family's own rule before it
// asks for a password, so a value the command would refuse is refused here
// without one.
void ChildrenPage::run(const QStringList& arguments, Step onDone) {
    const FamilyRequest request = FamilyRequest::fromArguments(arguments);
    if (request.refusal) {
        setStep(Step::NotChanged, reasonFor(refusalWord(*request.refusal)));
        return;
    }
    // Asking comes first: when the process cannot start at all, its answer
    // arrives inside start(), and must not be overwritten.
    const Step before = m_step;
    const Reason reasonBefore = m_reason;
    m_onDone = onDone;
    setStep(Step::Asking);
    if (!m_change->start(arguments)) {
        setStep(before, reasonBefore);
    }
}

// Whatever the answer, the list is read again: a refusal such as
// not-a-child means it was out of date.
void ChildrenPage::finished(const QString& answer) {
    reload();
    if (m_onDone == Step::Added) {
        rememberAdd(answer);
    }
    if (answer == QLatin1String("done")) {
        setStep(m_onDone);
    } else {
        setStep(Step::NotChanged, reasonFor(answer));
    }
}

// A failed add, or one with an answer the window cannot read, may have
// made the account before it stopped. It is remembered until an add with
// that login is done. A refusal or a closed password box changes nothing.
void ChildrenPage::rememberAdd(const QString& answer) {
    if (answer == QLatin1String("done")) {
        if (m_unfinished && m_unfinished->login == m_adding.login) {
            m_unfinished.reset();
        }
    } else if (answer == QLatin1String("failed") || reasonFor(answer) == Reason::Unknown) {
        m_unfinished = m_adding;
    }
}

void ChildrenPage::setStep(Step step, Reason reason) {
    m_step = step;
    m_reason = reason;
    emit stepChanged();
}
