// SPDX-License-Identifier: Apache-2.0
#include "ChildrenPage.h"

#include "ChildLanguage.h"
#include "FamilyChange.h"
#include "FamilyRequest.h"
#include "LoginName.h"

#include <QHash>

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

QString ChildrenPage::newName() const {
    return m_newName;
}

QString ChildrenPage::newLanguage() const {
    return m_newLanguage;
}

QString ChildrenPage::clash() const {
    return m_clash;
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

// A name a child on the list already has is refused, unless the add
// finishes a failed one: two children with one name look the same on
// every screen (#212). So is a grown-up's name, which would make the child
// look like that grown-up (#292). The login name comes from the name, or
// from the short name when the name gives none, and is never one an
// account or group already has: given an existing child's login,
// create-child would change that child instead.
void ChildrenPage::addChild(const QString& name, const QString& shortName, int level) {
    if (busy()) {
        return;
    }
    const QString fullName = name.trimmed();
    m_subject = fullName;
    m_level = level;
    m_clash.clear();
    if (fullName.isEmpty()) {
        setStep(Step::NotChanged, Reason::NoName);
        return;
    }
    const std::optional<QString> unfinished =
        m_unfinished.loginFor(fullName, m_children, m_setup.nameInUse);
    const ListedChild* other = unfinished ? nullptr : childNamed(m_children, fullName);
    if (refusedAsTaken(other, Reason::SameName, fullName)) {
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
    std::optional<QString> free = unfinished;
    if (!free) {
        free = freeLogin(*login, m_setup.nameInUse);
    }
    if (!free) {
        setStep(Step::NotChanged, Reason::Taken);
        return;
    }
    m_unfinished.starting(*free, fullName);
    run({QStringLiteral("create-child"), *free, QString::number(level),
         QStringLiteral("--full-name"), fullName},
        Step::Added);
}

// Only a child the page lists, so the page never names another account.
void ChildrenPage::changeLevel(const QString& login, int level) {
    if (busy()) {
        return;
    }
    const ListedChild* child = childWithLogin(m_children, login);
    m_subject = child == nullptr ? login : child->name;
    m_level = level;
    if (child == nullptr) {
        setStep(Step::NotChanged, Reason::NotAChild);
        return;
    }
    run({QStringLiteral("set-level"), login, QString::number(level)}, Step::LevelChanged);
}

// A child's new name, at any level, refused before any password when it is
// empty, is the child's full name already, or is another child's or a
// grown-up's (#212, #292). The same name in other capitals is a change, and
// so is a child's login given as the full name they never had.
void ChildrenPage::changeName(const QString& login, const QString& name) {
    if (busy()) {
        return;
    }
    const ListedChild* child = childWithLogin(m_children, login);
    m_subject = child == nullptr ? login : child->name;
    m_newName = name.trimmed();
    m_level = child == nullptr ? 0 : child->level;
    m_clash.clear();
    if (child == nullptr) {
        setStep(Step::NotChanged, Reason::NotAChild);
    } else if (m_newName.isEmpty()) {
        setStep(Step::NotChanged, Reason::NoName);
    } else if (m_newName == child->fullName) {
        setStep(Step::NotChanged, Reason::NameAsBefore);
    } else if (!refusedAsTaken(childNamed(m_children, m_newName, login), Reason::AnotherChilds,
                               m_newName)) {
        run({QStringLiteral("set-name"), login, m_newName}, Step::NameChanged);
    }
}

// A child's language, at any level, refused before any password when it is
// the one chosen for them already. A language Family does not offer is
// refused by cairn-family's own rule in run() (ADR-0058).
void ChildrenPage::changeLanguage(const QString& login, const QString& locale) {
    if (busy()) {
        return;
    }
    const ListedChild* child = childWithLogin(m_children, login);
    const ChildLanguage::Language* language = ChildLanguage::find(locale);
    m_subject = child == nullptr ? login : child->name;
    m_level = child == nullptr ? 0 : child->level;
    m_newLanguage = language == nullptr ? locale : ChildLanguage::nameOf(*language);
    if (child == nullptr) {
        setStep(Step::NotChanged, Reason::NotAChild);
    } else if (language != nullptr && locale == child->language) {
        setStep(Step::NotChanged, Reason::LanguageAsBefore);
    } else {
        run({QStringLiteral("set-language"), login, locale}, Step::LanguageChanged);
    }
}

// Refuses a name another child, given as `other`, or a grown-up has
// already, and says whether it did. The list is as last read, so a name
// given meanwhile from another window goes through, as in Add a child.
bool ChildrenPage::refusedAsTaken(const ListedChild* other, Reason childs, const QString& name) {
    if (other != nullptr) {
        m_clash = other->name;
        setStep(Step::NotChanged, childs);
        return true;
    }
    if (const std::optional<QString> grownUp = nameAmong(m_grownUps, name)) {
        m_clash = *grownUp;
        setStep(Step::NotChanged, Reason::AGrownUpsName);
        return true;
    }
    return false;
}

void ChildrenPage::forgetRefusal() {
    if (m_step == Step::NotChanged) {
        setStep(Step::Ready);
    }
}

void ChildrenPage::reload() {
    const std::vector<Account> accounts = m_setup.accounts();
    m_children = childrenAmong(accounts);
    for (ListedChild& child : m_children) {
        child.language = m_setup.languageOf(child.login).value_or(QString());
    }
    m_grownUps = grownUpNamesAmong(accounts);
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
        {QStringLiteral("bad-language"), Reason::BadLanguage},
        {QStringLiteral("taken"), Reason::Taken},
        {QStringLiteral("not-a-child"), Reason::NotAChild},
        {QStringLiteral("failed"), Reason::Failed},
        {QStringLiteral("busy"), Reason::Busy},
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
        m_unfinished.answered(answer, reasonFor(answer) != Reason::Unknown);
    }
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
