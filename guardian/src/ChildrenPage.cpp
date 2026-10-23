// SPDX-License-Identifier: Apache-2.0
#include "ChildrenPage.h"

#include "FamilyChange.h"
#include "FamilyRequest.h"
#include "FamilyRole.h"
#include "LoginName.h"

#include <QHash>
#include <QVariantMap>

#include <algorithm>
#include <utility>

ChildrenPage::ChildrenPage(Setup setup, QObject* parent)
    : QObject(parent), m_setup(std::move(setup)),
      m_change(new FamilyChange(m_setup.pkexec, m_setup.command, this)) {
    connect(m_change, &FamilyChange::finished, this, &ChildrenPage::finished);
    reload();
}

QVariantList ChildrenPage::listedChildren() const {
    QVariantList children;
    for (const ListedChild& child : m_children) {
        // Only the levels a child session exists for, so a child at L3 or
        // L4 is offered both.
        QVariantList otherLevels;
        for (int level = FamilyRole::lowestLevel; level <= FamilyRole::highestKioskLevel; ++level) {
            if (level != child.level) {
                otherLevels.append(level);
            }
        }
        children.append(QVariantMap{
            {QStringLiteral("login"), child.login},
            {QStringLiteral("name"), child.name},
            {QStringLiteral("level"), child.level},
            {QStringLiteral("otherLevels"), otherLevels},
        });
    }
    return children;
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
    const std::optional<QString> free = freeLogin(*login, m_setup.nameInUse);
    if (!free) {
        setStep(Step::NotChanged, Reason::Taken);
        return;
    }
    run({QStringLiteral("create-child"), *free, QString::number(level),
         QStringLiteral("--full-name"), fullName},
        Step::Added);
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
    if (!m_change->start(arguments)) {
        return;
    }
    m_onDone = onDone;
    setStep(Step::Asking);
}

// Whatever the answer, the list is read again: a refusal such as
// not-a-child means it was out of date.
void ChildrenPage::finished(const QString& answer) {
    reload();
    if (answer == QLatin1String("done")) {
        setStep(m_onDone);
    } else {
        setStep(Step::NotChanged, reasonFor(answer));
    }
}

void ChildrenPage::setStep(Step step, Reason reason) {
    m_step = step;
    m_reason = reason;
    emit stepChanged();
}
