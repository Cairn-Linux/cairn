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

// A page whose cairn-family is the pkexec stand-in's scenario, over a table
// of accounts the test may change, and that writes down each run's
// arguments.
class StandInFamily {
public:
    explicit StandInFamily(const QString& scenario) {
        qputenv("CAIRN_TEST_ARGUMENTS", record().toLocal8Bit());
        ChildrenPage::Setup setup;
        setup.pkexec = QStringLiteral(FIXTURES "/pkexec-family");
        setup.command = scenario;
        setup.accounts = [this] { return accounts; };
        setup.nameInUse = [this](const QString& name) {
            return groups.contains(name) ||
                   std::ranges::any_of(
                       accounts, [&name](const Account& account) { return account.login == name; });
        };
        page = std::make_unique<ChildrenPage>(setup);
    }

    ~StandInFamily() {
        qunsetenv("CAIRN_TEST_ARGUMENTS");
    }
    StandInFamily(const StandInFamily&) = delete;
    StandInFamily& operator=(const StandInFamily&) = delete;
    StandInFamily(StandInFamily&&) = delete;
    StandInFamily& operator=(StandInFamily&&) = delete;

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

    // Never an existing child's login: create-child would change that child.
    void aSecondAdaGetsANewLogin() {
        StandInFamily family(QStringLiteral("done"));
        family.page->addChild(QStringLiteral("Ada"), QString(), 1);
        QVERIFY(family.waitUntilAnswered());
        QCOMPARE(family.runs().value(0).value(1), QStringLiteral("ada2"));
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
        QTest::newRow("level 3") << "Ada" << 3 << ChildrenPage::Reason::BadLevel;
        QTest::newRow("level 0") << "Ada" << 0 << ChildrenPage::Reason::BadLevel;
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
