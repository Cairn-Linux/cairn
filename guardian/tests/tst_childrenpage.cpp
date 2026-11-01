// SPDX-License-Identifier: Apache-2.0
#include "ChildrenPage.h"

#include <QFile>
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
        setup.accounts = [this] { return accounts; };
        setup.nameInUse = [this](const QString& name) {
            return groups.contains(name) ||
                   std::ranges::any_of(
                       accounts, [&name](const Account& account) { return account.login == name; });
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
    std::unique_ptr<ChildrenPage> page;

private:
    QTemporaryDir m_folder;
    ChildrenPage::Setup m_setup;
};

QStringList loginsOf(const QVariantList& children) {
    QStringList logins;
    for (const QVariant& child : children) {
        logins.append(child.toMap().value(QStringLiteral("login")).toString());
    }
    return logins;
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
    void aNameAChildHasIsRefused_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("subject");

        QTest::newRow("the same") << "Ada" << "Ada";
        QTest::newRow("small letters") << "ada" << "ada";
        QTest::newRow("capitals and spaces") << "  BEN " << "BEN";
        QTest::newRow("a login as the name") << "Dee" << "Dee";
        QTest::newRow("another alphabet") << QStringLiteral("анна") << QStringLiteral("анна");
    }

    void aNameAChildHasIsRefused() {
        QFETCH(const QString, name);
        QFETCH(const QString, subject);
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
        QVERIFY(!family.page->busy());
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
        QCOMPARE(family.page->step(), ChildrenPage::Step::Asking);
        QCOMPARE(family.page->subject(), QStringLiteral("Ada"));
        QTest::qWait(300);
        QCOMPARE(family.runs(), QList<QStringList>({{"set-level", "ada", "2"}}));
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
        QTest::newRow("taken") << "refused-taken" << Reason::Taken;
        QTest::newRow("not-a-child") << "refused-not-a-child" << Reason::NotAChild;
        QTest::newRow("failed") << "refused-failed" << Reason::Failed;
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
