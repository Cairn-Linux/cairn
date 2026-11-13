// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "ChildList.h"
#include "LanguageFile.h"
#include "UnfinishedAdd.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

class FamilyChange;

// What the Guardian tool's Children page shows, and what its buttons do:
// the family's children with their levels, adding a child, moving a child
// to another level (ADR-0040), and changing a child's name (ADR-0056) and
// language (ADR-0058).
// Every change is one run of cairn-family through pkexec (ADR-0039).
class ChildrenPage : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The window is given one by main().")
    // Each child as a map: login, name, level, otherLevels, the levels the
    // page offers to move them to, languageName, the name of the language
    // chosen for them or "", and otherLanguages, the languages it offers
    // them, each a map of locale and name.
    Q_PROPERTY(QVariantList listedChildren READ listedChildren NOTIFY listedChildrenChanged)
    Q_PROPERTY(Step step READ step NOTIFY stepChanged)
    Q_PROPERTY(Reason reason READ reason NOTIFY stepChanged)
    // The name of the child the last change was for, and the level it gave.
    Q_PROPERTY(QString subject READ subject NOTIFY stepChanged)
    // The name the last change of a name asked for, as typed but for spaces
    // at either end.
    Q_PROPERTY(QString newName READ newName NOTIFY stepChanged)
    // The name of the language the last change of a language asked for.
    Q_PROPERTY(QString newLanguage READ newLanguage NOTIFY stepChanged)
    // The name, as the family knows it, of the child or grown-up a name the
    // page refused already belongs to.
    Q_PROPERTY(QString clash READ clash NOTIFY stepChanged)
    Q_PROPERTY(int level READ level NOTIFY stepChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stepChanged)

public:
    enum class Step : std::uint8_t {
        Ready,
        Asking, // pkexec asks for the password, then the command runs
        Added,
        LevelChanged,
        NameChanged,
        LanguageChanged,
        NotChanged, // see reason()
    };
    Q_ENUM(Step)

    // Why nothing changed. The first seven the page finds itself; the rest
    // are cairn-family's own words (ADR-0039) and pkexec's.
    enum class Reason : std::uint8_t {
        None,
        NoName,
        NoShortName,
        SameName,         // a child on the list has that name already (#212)
        AnotherChilds,    // a new name another child on the list has (#212)
        AGrownUpsName,    // a name a grown-up's account has (#292)
        NameAsBefore,     // a new name that is the child's name already
        LanguageAsBefore, // the language the child has already (#299)
        NotRoot,
        NotAGrownUp,
        Usage,
        BadName,
        BadFullName,
        BadLevel,
        BadLanguage,
        Taken,
        NotAChild,
        Failed,
        Busy, // another change to the family was under way (#255)
        Dismissed,
        NotAuthorized,
        Unknown,
    };
    Q_ENUM(Reason)

    struct Setup {
        QString pkexec = QStringLiteral("/usr/bin/pkexec");
        // cairn-family; main() names the one this build installs.
        QString command;
        std::function<std::vector<Account>()> accounts = listAccounts;
        std::function<bool(const QString&)> nameInUse = ::nameInUse;
        // The language a Guardian chose for the child with this login.
        std::function<std::optional<QString>(const QString&)> languageOf =
            [](const QString& login) {
                return LanguageFile::read(QString::fromLatin1(LanguageFile::folder), login);
            };
    };

    explicit ChildrenPage(Setup setup, QObject* parent = nullptr);

    QVariantList listedChildren() const;
    Step step() const;
    Reason reason() const;
    QString subject() const;
    QString newName() const;
    QString newLanguage() const;
    QString clash() const;
    int level() const;
    bool busy() const;

    // Whether a short name must be typed too, because the name gives no
    // login name on its own. A member, not static, because QML calls it on
    // the page and cannot call a static one.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    Q_INVOKABLE bool needsShortName(const QString& name) const;
    Q_INVOKABLE void addChild(const QString& name, const QString& shortName, int level);
    Q_INVOKABLE void changeLevel(const QString& login, int level);
    Q_INVOKABLE void changeName(const QString& login, const QString& name);
    Q_INVOKABLE void changeLanguage(const QString& login, const QString& locale);
    // Forgets a refusal once the field it was about has closed, so the page
    // no longer shows it. A change that went through still shows.
    Q_INVOKABLE void forgetRefusal();
    // Reads the children again from the account database.
    Q_INVOKABLE void reload();

signals:
    void listedChildrenChanged();
    void stepChanged();

private:
    bool refusedAsTaken(const ListedChild* other, Reason childs, const QString& name);
    void run(const QStringList& arguments, Step onDone);
    void finished(const QString& answer);
    void setStep(Step step, Reason reason = Reason::None);

    Setup m_setup;
    FamilyChange* m_change;
    std::vector<ListedChild> m_children;
    QStringList m_grownUps;
    Step m_step = Step::Ready;
    Reason m_reason = Reason::None;
    QString m_subject;
    QString m_newName;
    QString m_newLanguage;
    QString m_clash;
    int m_level = 0;
    Step m_onDone = Step::Ready;
    UnfinishedAdd m_unfinished;
};
