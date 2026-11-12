// SPDX-License-Identifier: Apache-2.0
#include "ChildrenPage.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

namespace {

constexpr int kAnswerMilliseconds = 10000;

// The accounts a page sees: a Guardian, an administrator, a plain account
// and two children, listed out of order as NSS may.
std::vector<Account> family() {
    return {
        {.uid = 1003, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = "Ben"},
        {.uid = 1000,
         .groups = {"guardian", "wheel", "cairn-guardian"},
         .login = "guardian",
         .fullName = "Guardian"},
        {.uid = 1001, .groups = {"helper", "wheel"}, .login = "helper", .fullName = {}},
        {.uid = 1002, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"},
        {.uid = 1004, .groups = {"guest"}, .login = "guest", .fullName = {}},
    };
}

// A page whose pkexec and cairn-family are the stand-ins, answering as the
// scenario says, over a table of accounts the test may change, and that
// writes down each run's arguments.
class StandInFamily {
public:
    explicit StandInFamily(const QString& scenario, ChildrenPage::Setup setup = {}) {
        qputenv("CAIRN_TEST_ARGUMENTS", record().toLocal8Bit());
        answerWith(scenario);
        if (setup.command.isEmpty()) {
            setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
            setup.command = QStringLiteral(FIXTURES "/cairn-family");
        }
        setup.accounts = [this] {
            ++reads;
            return accounts;
        };
        setup.nameInUse = [this](const QString& name) {
            return groups.contains(name) ||
                   std::ranges::any_of(
                       accounts, [&name](const Account& account) { return account.login == name; });
        };
        setup.languageOf = [this](const QString& login) -> std::optional<QString> {
            const auto found = languages.constFind(login);
            return found == languages.constEnd() ? std::nullopt : std::optional(*found);
        };
        m_setup = setup;
        page = std::make_unique<ChildrenPage>(m_setup);
    }

    // The window closed and opened again: a new page, over the same
    // accounts and stand-ins, that remembers nothing of the old one.
    void reopen() {
        page = std::make_unique<ChildrenPage>(m_setup);
    }

    ~StandInFamily() {
        qunsetenv("CAIRN_TEST_ARGUMENTS");
        qunsetenv("CAIRN_TEST_SCENARIO");
    }
    StandInFamily(const StandInFamily&) = delete;
    StandInFamily& operator=(const StandInFamily&) = delete;
    StandInFamily(StandInFamily&&) = delete;
    StandInFamily& operator=(StandInFamily&&) = delete;

    // How the stand-ins answer the next run.
    static void answerWith(const QString& scenario) {
        qputenv("CAIRN_TEST_SCENARIO", scenario.toLocal8Bit());
    }

    QString record() const {
        return m_folder.filePath(QStringLiteral("arguments"));
    }

    // Every run so far, each as its arguments.
    QList<QStringList> runs() const {
        QFile file(record());
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        QList<QStringList> runs;
        for (const QString& line : QString::fromUtf8(file.readAll()).split('\n')) {
            if (line == QLatin1String("run")) {
                runs.append(QStringList{});
            } else if (!line.isEmpty() && !runs.isEmpty()) {
                runs.last().append(line);
            }
        }
        return runs;
    }

    bool waitUntilAnswered() const {
        return QTest::qWaitFor([this] { return !page->busy(); }, kAnswerMilliseconds);
    }

    std::vector<Account> accounts = family();
    QSet<QString> groups{"wheel", "cairn-l1", "cairn-l2", "cairn-guardian"};
    // Each child's language file, as LanguageFile::read() gives it.
    QHash<QString, QString> languages{{"ben", "en_GB.UTF-8"}};
    // How many times the page has read the accounts.
    int reads = 0;
    std::unique_ptr<ChildrenPage> page;

private:
    QTemporaryDir m_folder;
    ChildrenPage::Setup m_setup;
};

// The account is gone, as after `userdel` in a terminal.
void removeAccount(std::vector<Account>& accounts, const QString& login) {
    std::erase_if(accounts, [&login](const Account& account) { return account.login == login; });
}

QStringList loginsOf(const QVariantList& children) {
    QStringList logins;
    for (const QVariant& child : children) {
        logins.append(child.toMap().value(QStringLiteral("login")).toString());
    }
    return logins;
}

// The locales the page offers a child, from their map.
QStringList localesOf(const QVariant& child) {
    QStringList locales;
    for (const QVariant& language :
         child.toMap().value(QStringLiteral("otherLanguages")).toList()) {
        locales.append(language.toMap().value(QStringLiteral("locale")).toString());
    }
    return locales;
}

class ChildrenPageTest : public QObject {
    Q_OBJECT

private slots:
    // Only the children, in the order their accounts were made.
    void itListsTheChildren() {
        StandInFamily family(QStringLiteral("done"));
        const QVariantList children = family.page->listedChildren();
        QCOMPARE(loginsOf(children), QStringList({"ada", "ben"}));
        const QVariantMap ada = children.at(0).toMap();
        QCOMPARE(ada.value("name").toString(), QStringLiteral("Ada"));
        QCOMPARE(ada.value("level").toInt(), 1);
        QCOMPARE(ada.value("otherLevels").toList(), QVariantList{QVariant(2)});
        QCOMPARE(children.at(1).toMap().value("otherLevels").toList(), QVariantList{QVariant(1)});
        QCOMPARE(family.page->step(), ChildrenPage::Step::Ready);
    }

    // A child at L3 keeps it on the list, and may be moved to 1 or 2.
    void anOlderChildIsOfferedOneAndTwo() {
        StandInFamily family(QStringLiteral("done"));
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l3"}, .login = "cy", .fullName = {}});
        family.page->reload();
        const QVariantMap cy = family.page->listedChildren().at(2).toMap();
        QCOMPARE(cy.value("level").toInt(), 3);
        QCOMPARE(cy.value("otherLevels").toList(), (QVariantList{QVariant(1), QVariant(2)}));
    }

    void addAChild() {
        StandInFamily family(QStringLiteral("done"));
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        family.page->addChild(QStringLiteral("  Zoë  "), QString(), 2);
        QCOMPARE(family.page->step(), ChildrenPage::Step::Asking);
        QVERIFY(family.page->busy());
        // What the command makes, the page reads back once it is done.
        family.accounts.push_back(
            {.uid = 1005, .groups = {"zoe", "cairn-l2"}, .login = "zoe", .fullName = "Zoë"});
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::Added);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::None);
        QCOMPARE(family.page->subject(), QStringLiteral("Zoë"));
        QCOMPARE(family.page->level(), 2);
        QCOMPARE(family.runs(), QList<QStringList>({{"create-child", "zoe", "2", "--full-name",
                                                     QStringLiteral("Zoë")}}));
        QCOMPARE(listed.size(), 1);
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada", "ben", "zoe"}));
    }

    // Never an existing child's login: create-child would change that
    // child. Cy is not Cy B's name, but makes Cy B's login.
    void anotherChildsLoginIsNotUsed() {
        StandInFamily family(QStringLiteral("done"));
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy B"});
        family.page->reload();
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::Added);
        QCOMPARE(family.runs().value(0).value(1), QStringLiteral("cy2"));
    }

    // A name a child on the list has, whatever the case of its letters, is
    // refused before the password is asked for, with the name as typed. A
    // child with no full name is listed, and named on the login screen, by
    // their login. A missing short name is not asked for first.
    // The clash is the other child's name as the family knows it.
    void aNameAChildHasIsRefused_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("subject");
        QTest::addColumn<QString>("clash");

        QTest::newRow("the same") << "Ada" << "Ada" << "Ada";
        QTest::newRow("small letters") << "ada" << "ada" << "Ada";
        QTest::newRow("capitals and spaces") << "  BEN " << "BEN" << "Ben";
        QTest::newRow("a login as the name") << "Dee" << "Dee" << "dee";
        QTest::newRow("another alphabet")
            << QStringLiteral("анна") << QStringLiteral("анна") << QStringLiteral("Анна");
    }

    void aNameAChildHasIsRefused() {
        QFETCH(const QString, name);
        QFETCH(const QString, subject);
        QFETCH(const QString, clash);
        StandInFamily family(QStringLiteral("done"));
        family.accounts.push_back(
            {.uid = 1005, .groups = {"dee", "cairn-l2"}, .login = "dee", .fullName = {}});
        family.accounts.push_back({.uid = 1006,
                                   .groups = {"anna", "cairn-l1"},
                                   .login = "anna",
                                   .fullName = QStringLiteral("Анна")});
        family.page->reload();
        family.page->addChild(name, QString(), 1);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::SameName);
        QCOMPARE(family.page->subject(), subject);
        QCOMPARE(family.page->clash(), clash);
        QVERIFY(!family.page->busy());
        QVERIFY(family.runs().isEmpty());
    }

    // A grown-up's name, full name or login when they have none, is refused
    // too: the child would look like that grown-up on every screen (#292).
    void aGrownUpsNameIsRefused_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("clash");

        QTest::newRow("a Guardian's") << "guardian " << "Guardian";
        QTest::newRow("an administrator's login") << "Helper" << "helper";
    }

    void aGrownUpsNameIsRefused() {
        QFETCH(const QString, name);
        QFETCH(const QString, clash);
        StandInFamily family(QStringLiteral("done"));
        family.page->addChild(name, QString(), 1);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::AGrownUpsName);
        QCOMPARE(family.page->clash(), clash);
        family.page->changeName(QStringLiteral("ada"), name);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::AGrownUpsName);
        QCOMPARE(family.page->clash(), clash);
        QVERIFY(family.runs().isEmpty());
    }

    // useradd makes a group of the same name, so a group blocks it too.
    void aGroupsNameIsNotUsed() {
        StandInFamily family(QStringLiteral("done"));
        family.groups.insert(QStringLiteral("dee"));
        family.page->addChild(QStringLiteral("Dee"), QString(), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.runs().value(0).value(1), QStringLiteral("dee2"));
    }

    void aNameWithNoLettersNeedsAShortName() {
        StandInFamily family(QStringLiteral("done"));
        QVERIFY(!family.page->needsShortName(QString()));
        QVERIFY(!family.page->needsShortName(QStringLiteral("Ada")));
        QVERIFY(family.page->needsShortName(QStringLiteral("Анна")));

        family.page->addChild(QStringLiteral("Анна"), QString(), 1);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::NoShortName);

        family.page->addChild(QStringLiteral("Анна"), QStringLiteral("Anna"), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::Added);
        QCOMPARE(family.runs(), QList<QStringList>({{"create-child", "anna", "1", "--full-name",
                                                     QStringLiteral("Анна")}}));
    }

    // Refused before the password is asked for, with cairn-family's rule.
    void refusedWithoutRunning_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<int>("level");
        QTest::addColumn<ChildrenPage::Reason>("reason");

        QTest::newRow("empty") << QString() << 1 << ChildrenPage::Reason::NoName;
        QTest::newRow("only spaces") << "   " << 1 << ChildrenPage::Reason::NoName;
        QTest::newRow("colon") << "Ada: L" << 1 << ChildrenPage::Reason::BadFullName;
        QTest::newRow("comma") << "Ada, L" << 1 << ChildrenPage::Reason::BadFullName;
        QTest::newRow("backslash") << "Ada\\L" << 1 << ChildrenPage::Reason::BadFullName;
        QTest::newRow("line break") << "Ada\nL" << 1 << ChildrenPage::Reason::BadFullName;
        QTest::newRow("level 3") << "Dee" << 3 << ChildrenPage::Reason::BadLevel;
        QTest::newRow("level 0") << "Dee" << 0 << ChildrenPage::Reason::BadLevel;
    }

    void refusedWithoutRunning() {
        QFETCH(const QString, name);
        QFETCH(const int, level);
        QFETCH(const ChildrenPage::Reason, reason);
        StandInFamily family(QStringLiteral("done"));
        family.page->addChild(name, QString(), level);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), reason);
        QVERIFY(!family.page->busy());
        QVERIFY(family.runs().isEmpty());
    }

    // The same call up or down, and the child's name for the sentence.
    void changeALevel() {
        StandInFamily family(QStringLiteral("done"));
        family.page->changeLevel(QStringLiteral("ben"), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::LevelChanged);
        QCOMPARE(family.page->subject(), QStringLiteral("Ben"));
        QCOMPARE(family.page->level(), 1);
        family.page->changeLevel(QStringLiteral("ada"), 2);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::LevelChanged);
        QCOMPARE(family.runs(),
                 QList<QStringList>({{"set-level", "ben", "1"}, {"set-level", "ada", "2"}}));
    }

    // Only a child the page lists, so it never names anyone else.
    void onlyAListedChildsLevel() {
        StandInFamily family(QStringLiteral("done"));
        for (const char* login : {"guardian", "helper", "guest", "nobody-here"}) {
            family.page->changeLevel(QString::fromLatin1(login), 2);
            QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
            QCOMPARE(family.page->reason(), ChildrenPage::Reason::NotAChild);
        }
        family.page->changeLevel(QStringLiteral("ada"), 3);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::BadLevel);
        QVERIFY(family.runs().isEmpty());
    }

    // A new name for a listed child, with both names for the sentence.
    // Spaces at either end go; other capitals are a change (ADR-0056).
    void changeAName() {
        StandInFamily family(QStringLiteral("done"));
        family.page->changeName(QStringLiteral("ben"), QStringLiteral("  Benjamin "));
        QCOMPARE(family.page->step(), ChildrenPage::Step::Asking);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NameChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::None);
        QCOMPARE(family.page->subject(), QStringLiteral("Ben"));
        QCOMPARE(family.page->newName(), QStringLiteral("Benjamin"));
        family.page->changeName(QStringLiteral("ada"), QStringLiteral("ADA"));
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NameChanged);
        QCOMPARE(family.runs(),
                 QList<QStringList>({{"set-name", "ben", "Benjamin"}, {"set-name", "ada", "ADA"}}));
    }

    // Each child with the language chosen for them, or none, and the
    // languages the page offers them: every one but theirs (ADR-0058).
    void eachChildsLanguage() {
        const StandInFamily family(QStringLiteral("done"));
        const QVariantList children = family.page->listedChildren();
        QCOMPARE(children.at(0).toMap().value("languageName").toString(), QString());
        QCOMPARE(localesOf(children.at(0)), QStringList({"en_US.UTF-8", "en_GB.UTF-8"}));
        QCOMPARE(children.at(1).toMap().value("languageName").toString(),
                 QStringLiteral("English (UK)"));
        QCOMPARE(localesOf(children.at(1)), QStringList({"en_US.UTF-8"}));
        const QVariantMap offered =
            children.at(1).toMap().value("otherLanguages").toList().at(0).toMap();
        QCOMPARE(offered.value("name").toString(), QStringLiteral("English (US)"));
    }

    // A file with a locale Family does not offer reads as none.
    void aLanguageNotOfferedIsNone() {
        StandInFamily family(QStringLiteral("done"));
        family.languages.insert("ada", "de_DE.UTF-8");
        family.page->reload();
        QCOMPARE(family.page->listedChildren().at(0).toMap().value("languageName").toString(),
                 QString());
        QCOMPARE(localesOf(family.page->listedChildren().at(0)),
                 QStringList({"en_US.UTF-8", "en_GB.UTF-8"}));
    }

    // One run of set-language, with both names for the sentence, and the
    // list read again after it.
    void changeALanguage() {
        StandInFamily family(QStringLiteral("done"));
        family.page->changeLanguage(QStringLiteral("ada"), QStringLiteral("en_GB.UTF-8"));
        QCOMPARE(family.page->step(), ChildrenPage::Step::Asking);
        family.languages.insert("ada", "en_GB.UTF-8");
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::LanguageChanged);
        QCOMPARE(family.page->subject(), QStringLiteral("Ada"));
        QCOMPARE(family.page->newLanguage(), QStringLiteral("English (UK)"));
        QCOMPARE(family.page->listedChildren().at(0).toMap().value("languageName").toString(),
                 QStringLiteral("English (UK)"));
        family.page->changeLanguage(QStringLiteral("ben"), QStringLiteral("en_US.UTF-8"));
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::LanguageChanged);
        QCOMPARE(family.runs(), QList<QStringList>({{"set-language", "ada", "en_GB.UTF-8"},
                                                    {"set-language", "ben", "en_US.UTF-8"}}));
    }

    // Refused before any password: the language the child has, one Family
    // does not offer, and anyone the page does not list.
    void aLanguageRefusedWithoutRunning_data() {
        QTest::addColumn<QString>("login");
        QTest::addColumn<QString>("locale");
        QTest::addColumn<ChildrenPage::Reason>("reason");

        using Reason = ChildrenPage::Reason;
        QTest::newRow("as before") << "ben" << "en_GB.UTF-8" << Reason::LanguageAsBefore;
        QTest::newRow("not offered") << "ada" << "de_DE.UTF-8" << Reason::BadLanguage;
        QTest::newRow("empty") << "ada" << "" << Reason::BadLanguage;
        QTest::newRow("a Guardian") << "guardian" << "en_GB.UTF-8" << Reason::NotAChild;
        QTest::newRow("a plain account") << "guest" << "en_GB.UTF-8" << Reason::NotAChild;
    }

    void aLanguageRefusedWithoutRunning() {
        QFETCH(const QString, login);
        QFETCH(const QString, locale);
        QFETCH(const ChildrenPage::Reason, reason);
        StandInFamily family(QStringLiteral("done"));
        family.page->changeLanguage(login, locale);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), reason);
        QVERIFY(family.runs().isEmpty());
    }

    // Closing the name field forgets a refusal, never a change made.
    void aRefusalIsForgottenAChangeIsNot() {
        StandInFamily family(QStringLiteral("done"));
        family.page->changeName(QStringLiteral("ada"), QStringLiteral("Ada"));
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::NameAsBefore);
        family.page->forgetRefusal();
        QCOMPARE(family.page->step(), ChildrenPage::Step::Ready);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::None);
        family.page->changeName(QStringLiteral("ben"), QStringLiteral("Benjamin"));
        QVERIFY(family.waitUntilAnswered());
        family.page->forgetRefusal();
        QCOMPARE(family.page->step(), ChildrenPage::Step::NameChanged);
    }

    // A child at level 3 has a name like any other.
    void anOlderChildsName() {
        StandInFamily family(QStringLiteral("done"));
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l3"}, .login = "cy", .fullName = {}});
        family.page->reload();
        family.page->changeName(QStringLiteral("cy"), QStringLiteral("Cy"));
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NameChanged);
        QCOMPARE(family.page->subject(), QStringLiteral("cy"));
        QCOMPARE(family.runs(), QList<QStringList>({{"set-name", "cy", "Cy"}}));
    }

    // A child with no full name is listed by their login, and may be given
    // it as the full name they never had.
    void aLoginGivenAsTheName() {
        StandInFamily family(QStringLiteral("done"));
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = {}});
        family.page->reload();
        family.page->changeName(QStringLiteral("cy"), QStringLiteral("cy"));
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NameChanged);
        QCOMPARE(family.runs(), QList<QStringList>({{"set-name", "cy", "cy"}}));
    }

    // The name the new one clashes with is the other child's, as the list
    // shows it, not as it was typed.
    void anotherChildsNameAsTheListShowsIt() {
        StandInFamily family(QStringLiteral("done"));
        family.page->changeName(QStringLiteral("ada"), QStringLiteral(" bEN"));
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::AnotherChilds);
        QCOMPARE(family.page->clash(), QStringLiteral("Ben"));
        QCOMPARE(family.page->newName(), QStringLiteral("bEN"));
    }

    // Refused before the password is asked for: no name, the name the child
    // has, another child's name in any capitals, cairn-family's rule, and
    // anyone the page does not list.
    void aNameRefusedWithoutRunning_data() {
        QTest::addColumn<QString>("login");
        QTest::addColumn<QString>("name");
        QTest::addColumn<ChildrenPage::Reason>("reason");
        QTest::addColumn<QString>("subject");

        using Reason = ChildrenPage::Reason;
        QTest::newRow("empty") << "ada" << QString() << Reason::NoName << "Ada";
        QTest::newRow("only spaces") << "ada" << "   " << Reason::NoName << "Ada";
        QTest::newRow("as before") << "ada" << " Ada " << Reason::NameAsBefore << "Ada";
        QTest::newRow("another child's") << "ada" << "Ben" << Reason::AnotherChilds << "Ada";
        QTest::newRow("another child's in small letters")
            << "ada" << "ben" << Reason::AnotherChilds << "Ada";
        QTest::newRow("another child's login, their name")
            << "ada" << "Dee" << Reason::AnotherChilds << "Ada";
        QTest::newRow("a Guardian's") << "ada" << "Guardian" << Reason::AGrownUpsName << "Ada";
        QTest::newRow("an administrator's login")
            << "ada" << "helper" << Reason::AGrownUpsName << "Ada";
        QTest::newRow("colon") << "ada" << "Ada: L" << Reason::BadFullName << "Ada";
        QTest::newRow("comma") << "ada" << "Ada, L" << Reason::BadFullName << "Ada";
        QTest::newRow("backslash") << "ada" << "Ada\\L" << Reason::BadFullName << "Ada";
        QTest::newRow("line break") << "ada" << "Ada\nL" << Reason::BadFullName << "Ada";
        QTest::newRow("a Guardian") << "guardian" << "Mum" << Reason::NotAChild << "guardian";
        QTest::newRow("an administrator") << "helper" << "Mum" << Reason::NotAChild << "helper";
        QTest::newRow("a plain account") << "guest" << "Mum" << Reason::NotAChild << "guest";
        QTest::newRow("no one") << "nobody-here" << "Mum" << Reason::NotAChild << "nobody-here";
    }

    void aNameRefusedWithoutRunning() {
        QFETCH(const QString, login);
        QFETCH(const QString, name);
        QFETCH(const ChildrenPage::Reason, reason);
        QFETCH(const QString, subject);
        StandInFamily family(QStringLiteral("done"));
        family.accounts.push_back(
            {.uid = 1005, .groups = {"dee", "cairn-l2"}, .login = "dee", .fullName = {}});
        family.page->reload();
        family.page->changeName(login, name);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), reason);
        QCOMPARE(family.page->subject(), subject);
        QVERIFY(!family.page->busy());
        QVERIFY(family.runs().isEmpty());
    }

    // Each of cairn-family's answers reads as for any change, and the list
    // is read again after it.
    void aNameRefusedByTheCommand() {
        StandInFamily family(QStringLiteral("refused-not-a-child"));
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        family.page->changeName(QStringLiteral("ada"), QStringLiteral("Ada Jones"));
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::NotAChild);
        QCOMPARE(listed.size(), 1);
    }

    // An add that failed may have made the account. Adding the same name
    // again finishes that one, with the same login, and makes no second.
    void addingAgainAfterAFailureFinishesTheFirst() {
        StandInFamily family(QStringLiteral("refused-failed"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        // useradd ran, then a later step failed.
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::Failed);
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada", "ben", "cy"}));

        StandInFamily::answerWith(QStringLiteral("done"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::Added);
        QCOMPARE(family.runs(),
                 QList<QStringList>({{"create-child", "cy", "1", "--full-name", "Cy"},
                                     {"create-child", "cy", "1", "--full-name", "Cy"}}));

        // Once it is done, Cy is a child like any other, so the name is
        // refused.
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::SameName);
        QCOMPARE(family.runs().size(), 2);
    }

    // The same name in other capitals finishes it too, with the name as
    // typed now.
    void addingAgainInOtherCapitalsFinishesTheFirst() {
        StandInFamily family(QStringLiteral("refused-failed"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        QVERIFY(family.waitUntilAnswered());

        StandInFamily::answerWith(QStringLiteral("done"));
        family.page->addChild(QStringLiteral(" CY "), QString(), 2);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::Added);
        QCOMPARE(family.runs().value(1),
                 QStringList({"create-child", "cy", "2", "--full-name", "CY"}));
    }

    // Closed after a failed add and opened again, the window has forgotten
    // it. The child the add made is on the list like any other, so the
    // name is refused, and no second Cy is made.
    void aWindowOpenedAgainRefusesTheName() {
        StandInFamily family(QStringLiteral("refused-failed"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        QVERIFY(family.waitUntilAnswered());

        family.reopen();
        StandInFamily::answerWith(QStringLiteral("done"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::SameName);
        QCOMPARE(family.runs().size(), 1);
    }

    // The same after an answer the window cannot read, and after a closed
    // password box in between. A different name is a different child.
    void anUnreadableAddIsRememberedToo() {
        StandInFamily family(QStringLiteral("garbage"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 2);
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l2"}, .login = "cy", .fullName = "Cy"});
        QVERIFY(family.waitUntilAnswered());
        StandInFamily::answerWith(QStringLiteral("dismissed"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 2);
        QVERIFY(family.waitUntilAnswered());
        StandInFamily::answerWith(QStringLiteral("done"));
        family.page->addChild(QStringLiteral("Dee"), QString(), 2);
        QVERIFY(family.waitUntilAnswered());
        family.page->addChild(QStringLiteral("Cy"), QString(), 2);
        QVERIFY(family.waitUntilAnswered());
        QStringList logins;
        for (const QStringList& run : family.runs()) {
            logins.append(run.value(1));
        }
        QCOMPARE(logins, QStringList({"cy", "cy", "dee", "cy"}));
    }

    // A busy add changed nothing, so it is not remembered: when the change
    // under way made a Cy, adding Cy again is refused as any same name is.
    void aBusyAddIsNotRemembered() {
        StandInFamily family(QStringLiteral("refused-busy"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::Busy);

        StandInFamily::answerWith(QStringLiteral("done"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::SameName);
        QCOMPARE(family.runs().size(), 1);
    }

    // Another account took the login meanwhile, so it is not used again:
    // an account that is not a child, or another child, whom create-child
    // would change.
    void anUnfinishedLoginNowSomeoneElsesIsNotUsed_data() {
        QTest::addColumn<QStringList>("groups");
        QTest::addColumn<QString>("fullName");

        QTest::newRow("not a child") << QStringList{"cy"} << QString();
        QTest::newRow("another child") << QStringList{"cy", "cairn-l1"} << QStringLiteral("Cy B");
    }

    void anUnfinishedLoginNowSomeoneElsesIsNotUsed() {
        QFETCH(const QStringList, groups);
        QFETCH(const QString, fullName);
        StandInFamily family(QStringLiteral("refused-failed"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QVERIFY(family.waitUntilAnswered());
        family.accounts.push_back(
            {.uid = 1005, .groups = groups, .login = "cy", .fullName = fullName});
        family.page->reload();
        StandInFamily::answerWith(QStringLiteral("done"));
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.runs().value(1).value(1), QStringLiteral("cy2"));
    }

    // When cairn-family is not there, pkexec would say 127 as for a wrong
    // password. The page says instead that the answer could not be read.
    void aMissingCommandIsNotAPassword() {
        ChildrenPage::Setup setup;
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
        setup.command = QStringLiteral(FIXTURES "/no-such-cairn-family");
        StandInFamily family(QStringLiteral("not-authorized"), setup);
        family.page->changeLevel(QStringLiteral("ada"), 2);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::Unknown);
        QVERIFY(family.runs().isEmpty());
    }

    // QProcess answers inside start() when it cannot even try, as with no
    // program; the page must not then wait for ever.
    void aChangeThatCannotStartEnds() {
        ChildrenPage::Setup setup;
        setup.pkexec = QString();
        setup.command = QStringLiteral(FIXTURES "/cairn-family");
        StandInFamily family(QStringLiteral("done"), setup);
        family.page->changeLevel(QStringLiteral("ada"), 2);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::Unknown);
        QVERIFY(!family.page->busy());
    }

    // While the password box is open, nothing else starts.
    void oneChangeAtATime() {
        StandInFamily family(QStringLiteral("asking"));
        family.page->changeLevel(QStringLiteral("ada"), 2);
        QVERIFY(family.page->busy());
        family.page->changeLevel(QStringLiteral("ben"), 1);
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        family.page->changeName(QStringLiteral("ben"), QStringLiteral("Benjamin"));
        QCOMPARE(family.page->step(), ChildrenPage::Step::Asking);
        QCOMPARE(family.page->subject(), QStringLiteral("Ada"));
        QTest::qWait(300);
        QCOMPARE(family.runs(), QList<QStringList>({{"set-level", "ada", "2"}}));
    }

    // A child removed outside Family, in a terminal, while the window is
    // open. Their row goes when the page is shown again, and their name and
    // login are free for a new child. Add a child reads the accounts
    // itself, so they are free even when nothing showed the page first.
    void aChildRemovedOutsideNoLongerHoldsTheirName_data() {
        QTest::addColumn<bool>("shownFirst");

        QTest::newRow("the page shown again first") << true;
        QTest::newRow("straight to Add a child") << false;
    }

    void aChildRemovedOutsideNoLongerHoldsTheirName() {
        QFETCH(const bool, shownFirst);
        StandInFamily family(QStringLiteral("done"));
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        family.reopen();
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada", "ben", "cy"}));
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        const QSignalSpy stepped(family.page.get(), &ChildrenPage::stepChanged);
        removeAccount(family.accounts, QStringLiteral("cy"));
        if (shownFirst) {
            family.page->shown();
            QCOMPARE(stepped.size(), 0);
        }
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QCOMPARE(family.page->step(), ChildrenPage::Step::Asking);
        // The row went once, before the password was asked for.
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada", "ben"}));
        QCOMPARE(listed.size(), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::Added);
        QCOMPARE(family.runs(),
                 QList<QStringList>({{"create-child", "cy", "1", "--full-name", "Cy"}}));
    }

    // Shown again with the family as it was, the page reads the accounts
    // and says nothing, so the window leaves every row as it is.
    void theSameFamilyIsNotListedAgain() {
        StandInFamily family(QStringLiteral("done"));
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        const QSignalSpy stepped(family.page.get(), &ChildrenPage::stepChanged);
        const int before = family.reads;
        family.page->shown();
        QCOMPARE(family.reads, before + 1);
        QCOMPARE(listed.size(), 0);
        QCOMPARE(stepped.size(), 0);
        QCOMPARE(family.page->step(), ChildrenPage::Step::Ready);
    }

    // Anything a row shows, changed outside Family, makes the list again,
    // once.
    void eachChangeOutsideIsListedOnce() {
        StandInFamily family(QStringLiteral("done"));
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        family.page->shown();
        QCOMPARE(listed.size(), 1);
        family.accounts.back().groups = QStringList({"cy", "cairn-l2"});
        family.page->shown();
        QCOMPARE(listed.size(), 2);
        family.accounts.back().fullName = QStringLiteral("Cyrus");
        family.page->shown();
        QCOMPARE(listed.size(), 3);
        family.languages.insert("cy", "en_US.UTF-8");
        family.page->shown();
        QCOMPARE(listed.size(), 4);
        family.page->shown();
        QCOMPARE(listed.size(), 4);
        const QVariantMap cy = family.page->listedChildren().at(2).toMap();
        QCOMPARE(cy.value("name").toString(), QStringLiteral("Cyrus"));
        QCOMPARE(cy.value("level").toInt(), 2);
        QCOMPARE(cy.value("languageName").toString(), QStringLiteral("English (US)"));
    }

    // While a change waits for its password or its answer, showing the page
    // reads nothing: the answer reads the accounts itself.
    void aWaitingChangeIsNotDisturbed() {
        StandInFamily family(QStringLiteral("asking"));
        family.page->changeLevel(QStringLiteral("ada"), 2);
        QVERIFY(family.page->busy());
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        const QSignalSpy stepped(family.page.get(), &ChildrenPage::stepChanged);
        const int before = family.reads;
        removeAccount(family.accounts, QStringLiteral("ben"));
        family.page->shown();
        QCOMPARE(family.reads, before);
        QCOMPARE(listed.size(), 0);
        QCOMPARE(stepped.size(), 0);
        QCOMPARE(family.page->step(), ChildrenPage::Step::Asking);
        QCOMPARE(family.page->subject(), QStringLiteral("Ada"));
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada", "ben"}));
    }

    // What the last change did, or why it did nothing, stays said when the
    // page is shown again, even when the list is made again.
    void anOutcomeStaysWhenThePageIsShown() {
        StandInFamily family(QStringLiteral("done"));
        family.page->changeLevel(QStringLiteral("ada"), 2);
        QVERIFY(family.waitUntilAnswered());
        const QSignalSpy stepped(family.page.get(), &ChildrenPage::stepChanged);
        removeAccount(family.accounts, QStringLiteral("ben"));
        family.page->shown();
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada"}));
        QCOMPARE(family.page->step(), ChildrenPage::Step::LevelChanged);
        QCOMPARE(family.page->subject(), QStringLiteral("Ada"));
        QCOMPARE(family.page->level(), 2);
        QCOMPARE(stepped.size(), 0);

        family.page->addChild(QStringLiteral("ada"), QString(), 1);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::SameName);
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        family.page->shown();
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada", "cy"}));
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::SameName);
        QCOMPARE(family.page->subject(), QStringLiteral("ada"));
        QCOMPARE(family.page->clash(), QStringLiteral("Ada"));
        QCOMPARE(stepped.size(), 1);
    }

    // Each change reads the accounts before it decides, so what the page
    // refuses before any password is refused for the family as it is now.
    // A name a child got outside Family is taken, for a new child and for
    // a new name.
    void aNameAChildGotOutsideIsRefused() {
        StandInFamily family(QStringLiteral("done"));
        family.accounts.push_back(
            {.uid = 1005, .groups = {"cy", "cairn-l1"}, .login = "cy", .fullName = "Cy"});
        family.page->addChild(QStringLiteral("cy"), QString(), 1);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::SameName);
        QCOMPARE(family.page->clash(), QStringLiteral("Cy"));
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada", "ben", "cy"}));
        family.accounts.back().fullName = QStringLiteral("Cyrus");
        family.page->changeName(QStringLiteral("ada"), QStringLiteral("cyrus"));
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::AnotherChilds);
        QCOMPARE(family.page->clash(), QStringLiteral("Cyrus"));
        QVERIFY(family.runs().isEmpty());
    }

    // A name freed outside Family may be given.
    void aNameFreedOutsideMayBeGiven() {
        StandInFamily family(QStringLiteral("done"));
        removeAccount(family.accounts, QStringLiteral("ben"));
        family.page->changeName(QStringLiteral("ada"), QStringLiteral("Ben"));
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NameChanged);
        QCOMPARE(family.runs(), QList<QStringList>({{"set-name", "ada", "Ben"}}));
    }

    // A grown-up added outside Family is on no row, so the list is not made
    // again, but their name is theirs.
    void aGrownUpAddedOutsideKeepsTheirName() {
        StandInFamily family(QStringLiteral("done"));
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        family.accounts.push_back({.uid = 1005,
                                   .groups = {"gran", "cairn-guardian"},
                                   .login = "gran",
                                   .fullName = "Gran"});
        family.page->changeName(QStringLiteral("ada"), QStringLiteral("Gran"));
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::AGrownUpsName);
        family.page->addChild(QStringLiteral("gran"), QString(), 1);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::AGrownUpsName);
        QCOMPARE(family.page->clash(), QStringLiteral("Gran"));
        QCOMPARE(listed.size(), 0);
        QVERIFY(family.runs().isEmpty());
    }

    // A child removed outside Family is no child to change, and their row
    // goes without a password asked for. The sentence still has the name
    // their row showed; once the row is gone, only their login is known.
    void aChildRemovedOutsideIsNotChanged() {
        StandInFamily family(QStringLiteral("done"));
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        removeAccount(family.accounts, QStringLiteral("ben"));
        family.page->changeLevel(QStringLiteral("ben"), 1);
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::NotAChild);
        QCOMPARE(family.page->subject(), QStringLiteral("Ben"));
        QCOMPARE(loginsOf(family.page->listedChildren()), QStringList({"ada"}));
        QCOMPARE(listed.size(), 1);
        family.page->changeLanguage(QStringLiteral("ben"), QStringLiteral("en_US.UTF-8"));
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::NotAChild);
        QCOMPARE(family.page->subject(), QStringLiteral("ben"));
        QVERIFY(family.runs().isEmpty());
    }

    // A child given another name outside Family is named by it.
    void aChildRenamedOutsideHasTheNewName() {
        StandInFamily family(QStringLiteral("done"));
        family.accounts.front().fullName = QStringLiteral("Benjamin");
        family.page->changeLevel(QStringLiteral("ben"), 1);
        QCOMPARE(family.page->subject(), QStringLiteral("Benjamin"));
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::LevelChanged);
    }

    // A language chosen for a child outside Family is the one they have.
    void aLanguageChosenOutsideIsAsBefore() {
        StandInFamily family(QStringLiteral("done"));
        family.languages.insert("ada", "en_US.UTF-8");
        family.page->changeLanguage(QStringLiteral("ada"), QStringLiteral("en_US.UTF-8"));
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), ChildrenPage::Reason::LanguageAsBefore);
        QCOMPARE(family.page->listedChildren().at(0).toMap().value("languageName").toString(),
                 QStringLiteral("English (US)"));
        QVERIFY(family.runs().isEmpty());
    }

    void eachAnswerHasAReason_data() {
        QTest::addColumn<QString>("scenario");
        QTest::addColumn<ChildrenPage::Reason>("reason");

        using Reason = ChildrenPage::Reason;
        QTest::newRow("not-root") << "refused-not-root" << Reason::NotRoot;
        QTest::newRow("not-a-grown-up") << "refused-not-a-grown-up" << Reason::NotAGrownUp;
        QTest::newRow("usage") << "refused-usage" << Reason::Usage;
        QTest::newRow("bad-name") << "refused-bad-name" << Reason::BadName;
        QTest::newRow("bad-full-name") << "refused-bad-full-name" << Reason::BadFullName;
        QTest::newRow("bad-level") << "refused-bad-level" << Reason::BadLevel;
        QTest::newRow("bad-language") << "refused-bad-language" << Reason::BadLanguage;
        QTest::newRow("taken") << "refused-taken" << Reason::Taken;
        QTest::newRow("not-a-child") << "refused-not-a-child" << Reason::NotAChild;
        QTest::newRow("failed") << "refused-failed" << Reason::Failed;
        QTest::newRow("busy") << "refused-busy" << Reason::Busy;
        QTest::newRow("a word it does not know") << "refused-moon" << Reason::Unknown;
        QTest::newRow("dismissed") << "dismissed" << Reason::Dismissed;
        QTest::newRow("not-authorized") << "not-authorized" << Reason::NotAuthorized;
        QTest::newRow("garbage") << "garbage" << Reason::Unknown;
        QTest::newRow("silent") << "silent" << Reason::Unknown;
    }

    // Each answer, for either change; the list is read again after each.
    void eachAnswerHasAReason() {
        QFETCH(const QString, scenario);
        QFETCH(const ChildrenPage::Reason, reason);
        StandInFamily family(scenario);
        const QSignalSpy listed(family.page.get(), &ChildrenPage::listedChildrenChanged);
        family.page->addChild(QStringLiteral("Cy"), QString(), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->step(), ChildrenPage::Step::NotChanged);
        QCOMPARE(family.page->reason(), reason);
        QCOMPARE(family.page->subject(), QStringLiteral("Cy"));
        family.page->changeLevel(QStringLiteral("ada"), 2);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.page->reason(), reason);
        QCOMPARE(listed.size(), 2);
        QCOMPARE(family.runs().size(), 2);
    }
};

} // namespace

QTEST_GUILESS_MAIN(ChildrenPageTest)
#include "tst_childrenpage.moc"
