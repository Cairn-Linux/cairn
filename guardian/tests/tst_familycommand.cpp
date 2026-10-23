// SPDX-License-Identifier: Apache-2.0
#include "FamilyCommand.h"
#include "SteamEntry.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace {

constexpr auto steamFor = "/usr/bin/cairn-steam-for";

// The family the tests know. Only ada and ben are children's names.
QHash<QString, Account> family() {
    return {
        {"ada", {.uid = 1002, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"}},
        {"ben", {.uid = 1003, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = {}}},
        {"gina",
         {.uid = 1001,
          .groups = {"gina", "wheel", "cairn-guardian"},
          .login = "gina",
          .fullName = {}}},
        {"dad", {.uid = 1005, .groups = {"dad", "cairn-guardian"}, .login = "dad", .fullName = {}}},
        {"bazzite",
         {.uid = 1000, .groups = {"bazzite", "wheel"}, .login = "bazzite", .fullName = {}}},
        {"lodger", {.uid = 1006, .groups = {"lodger", "users"}, .login = "lodger", .fullName = {}}},
        {"daemon", {.uid = 2, .groups = {"daemon", "cairn-l1"}, .login = "daemon", .fullName = {}}},
        {"admin-child",
         {.uid = 1007,
          .groups = {"admin-child", "wheel", "cairn-l1"},
          .login = "admin-child",
          .fullName = {}}},
        {"guardian-child",
         {.uid = 1008,
          .groups = {"guardian-child", "cairn-guardian", "cairn-l2"},
          .login = "guardian-child",
          .fullName = {}}},
    };
}

// Stand-ins for the system's tools, linked in a folder of their own, and a
// folder for the menu entries.
class StandIns {
public:
    StandIns() {
        for (const char* tool : {"useradd", "passwd", "gpasswd", "usermod", "restorecon"}) {
            QFile::link(QStringLiteral(FIXTURES "/system-tool"), m_folder.filePath(tool));
        }
    }

    FamilyCommand::Setup setup() const {
        FamilyCommand::Setup setup;
        setup.useradd = m_folder.filePath("useradd");
        setup.passwd = m_folder.filePath("passwd");
        setup.gpasswd = m_folder.filePath("gpasswd");
        setup.usermod = m_folder.filePath("usermod");
        setup.restorecon = m_folder.filePath("restorecon");
        setup.applications = applications();
        setup.steamFor = QString::fromLatin1(steamFor);
        setup.euid = 0;
        setup.pkexecUid = std::nullopt;
        const QHash<QString, Account> accounts = m_accounts;
        setup.lookUp = [accounts](const QString& name) -> std::optional<Account> {
            const auto found = accounts.constFind(name);
            return found == accounts.constEnd() ? std::nullopt : std::optional(*found);
        };
        setup.lookUpUid = [accounts](unsigned int uid) -> std::optional<Account> {
            for (const Account& account : accounts) {
                if (account.uid == uid) {
                    return account;
                }
            }
            return std::nullopt;
        };
        const bool locked = m_locked;
        setup.passwordLocked = [locked](const QString&) -> std::optional<bool> { return locked; };
        return setup;
    }

    std::optional<Refusal> run(const QStringList& arguments) const {
        return FamilyCommand(setup()).run(arguments).refusal;
    }

    QStringList calls() const {
        QFile file(m_folder.filePath("calls"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    }

    QString environment() const {
        QFile file(m_folder.filePath("environment"));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }

    void fail(const QString& tool) const {
        QFile file(m_folder.filePath("fail-" + tool));
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    QString applications() const {
        return m_folder.filePath("applications");
    }

    QString entry(const QString& login) const {
        QFile file(SteamEntry::path(applications(), login));
        return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    }

    QString restorecon(const QString& login) const {
        return "restorecon " + SteamEntry::path(applications(), login);
    }

    void writeEntry(const QString& login, const QString& text) const {
        QDir().mkpath(applications());
        QFile file(SteamEntry::path(applications(), login));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(text.toUtf8());
    }

    void setAccounts(const QHash<QString, Account>& accounts) {
        m_accounts = accounts;
    }

    void setLocked(bool locked) {
        m_locked = locked;
    }

private:
    QTemporaryDir m_folder;
    QHash<QString, Account> m_accounts = family();
    bool m_locked = true;
};

QHash<QString, Account> familyWith(const QString& login, const QStringList& groups) {
    QHash<QString, Account> accounts = family();
    accounts.insert(login, {.uid = 1002, .groups = groups, .login = login, .fullName = {}});
    return accounts;
}

class FamilyCommandTest : public QObject {
    Q_OBJECT

private slots:
    void onlyRootChangesAnything() {
        const StandIns standIns;
        FamilyCommand::Setup setup = standIns.setup();
        setup.euid = 1001;
        QCOMPARE(FamilyCommand(setup).run({"create-child", "ben", "1"}).refusal, Refusal::NotRoot);
        QCOMPARE(standIns.calls(), QStringList());
    }

    void pkexecMustHaveBeenStartedByAGrownUp_data() {
        QTest::addColumn<QString>("pkexecUid");
        QTest::addColumn<bool>("allowed");
        QTest::newRow("a Guardian") << "1001" << true;
        QTest::newRow("a Guardian without wheel") << "1005" << true;
        QTest::newRow("another administrator") << "1000" << true;
        QTest::newRow("a child") << "1003" << false;
        QTest::newRow("an administrator in a level group") << "1007" << true;
        QTest::newRow("a plain account") << "1006" << false;
        QTest::newRow("a system account in a level group") << "2" << false;
        QTest::newRow("no such account") << "4242" << false;
        QTest::newRow("not a number") << "gina" << false;
        QTest::newRow("a sign first") << "+1001" << false;
        QTest::newRow("a space after") << "1001 " << false;
        QTest::newRow("empty") << "" << false;
        QTest::newRow("too big") << "99999999999" << false;
    }

    void pkexecMustHaveBeenStartedByAGrownUp() {
        QFETCH(const QString, pkexecUid);
        QFETCH(const bool, allowed);
        const StandIns standIns;
        FamilyCommand::Setup setup = standIns.setup();
        setup.pkexecUid = pkexecUid;
        const std::optional<Refusal> refusal =
            FamilyCommand(setup).run({"set-level", "ada", "1"}).refusal;
        QCOMPARE(refusal, allowed ? std::nullopt : std::optional(Refusal::NotAGrownUp));
    }

    void argumentsItDoesNotTake_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<Refusal>("refusal");
        QTest::newRow("nothing") << QStringList{} << Refusal::Usage;
        QTest::newRow("another subcommand")
            << QStringList{"delete-child", "ada", "1"} << Refusal::Usage;
        QTest::newRow("no level") << QStringList{"create-child", "ada"} << Refusal::Usage;
        QTest::newRow("one too many")
            << QStringList{"set-level", "ada", "1", "2"} << Refusal::Usage;
        QTest::newRow("a full name for set-level")
            << QStringList{"set-level", "ada", "1", "--full-name", "Ada"} << Refusal::Usage;
        QTest::newRow("another option")
            << QStringList{"create-child", "ada", "1", "--groups", "wheel"} << Refusal::Usage;
        QTest::newRow("a full name with no value")
            << QStringList{"create-child", "ada", "1", "--full-name"} << Refusal::Usage;
        QTest::newRow("capitals") << QStringList{"create-child", "Ada", "1"} << Refusal::BadName;
        QTest::newRow("a digit first")
            << QStringList{"create-child", "1ada", "1"} << Refusal::BadName;
        QTest::newRow("shell syntax")
            << QStringList{"create-child", "ada;id", "1"} << Refusal::BadName;
        QTest::newRow("a path") << QStringList{"create-child", "../ada", "1"} << Refusal::BadName;
        QTest::newRow("an option") << QStringList{"create-child", "-ada", "1"} << Refusal::BadName;
        QTest::newRow("empty") << QStringList{"create-child", "", "1"} << Refusal::BadName;
        QTest::newRow("33 letters")
            << QStringList{"create-child", QString(33, 'a'), "1"} << Refusal::BadName;
        QTest::newRow("level 3") << QStringList{"create-child", "ada", "3"} << Refusal::BadLevel;
        QTest::newRow("level 4") << QStringList{"set-level", "ada", "4"} << Refusal::BadLevel;
        QTest::newRow("level 0") << QStringList{"set-level", "ada", "0"} << Refusal::BadLevel;
        QTest::newRow("a leading zero")
            << QStringList{"set-level", "ada", "01"} << Refusal::BadLevel;
        QTest::newRow("a word") << QStringList{"set-level", "ada", "one"} << Refusal::BadLevel;
        QTest::newRow("a colon in the full name")
            << QStringList{"create-child", "ben", "1", "--full-name", "Ben:0:0"}
            << Refusal::BadFullName;
        QTest::newRow("a comma in the full name")
            << QStringList{"create-child", "ben", "1", "--full-name", "Ben,Room 2"}
            << Refusal::BadFullName;
        QTest::newRow("a newline in the full name")
            << QStringList{"create-child", "ben", "1", "--full-name", "Ben\nName=x"}
            << Refusal::BadFullName;
        QTest::newRow("a backslash in the full name")
            << QStringList{"create-child", "ben", "1", "--full-name", "Ben\\n"}
            << Refusal::BadFullName;
        QTest::newRow("an empty full name")
            << QStringList{"create-child", "ben", "1", "--full-name", " "} << Refusal::BadFullName;
    }

    void argumentsItDoesNotTake() {
        QFETCH(const QStringList, arguments);
        QFETCH(const Refusal, refusal);
        const StandIns standIns;
        QCOMPARE(standIns.run(arguments), std::optional(refusal));
        QCOMPARE(standIns.calls(), QStringList());
        QVERIFY(!QDir(standIns.applications()).exists());
    }

    void longestPlainNamesAreTaken() {
        StandIns standIns;
        standIns.setAccounts({});
        QCOMPARE(standIns.run({"create-child", QString(32, 'a'), "1"}), std::nullopt);
        QCOMPARE(standIns.run({"create-child", "_a-b_2", "2"}), std::nullopt);
    }

    void aNewChild() {
        StandIns standIns;
        standIns.setAccounts({});
        QCOMPARE(standIns.run({"create-child", "ada", "1"}), std::nullopt);
        QCOMPARE(standIns.calls(), QStringList({"useradd --create-home --groups cairn-l1 ada",
                                                "passwd --lock ada", standIns.restorecon("ada")}));
        QCOMPARE(standIns.entry("ada"), SteamEntry::text("ada", "ada", steamFor));
        QCOMPARE(QFile::permissions(SteamEntry::path(standIns.applications(), "ada")),
                 QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser |
                     QFileDevice::WriteUser | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }

    void aNewChildWithAFullName() {
        StandIns standIns;
        standIns.setAccounts({});
        QCOMPARE(standIns.run({"create-child", "ben", "2", "--full-name", "Ben Jones"}),
                 std::nullopt);
        QCOMPARE(standIns.calls().constFirst(),
                 QStringLiteral("useradd --create-home --comment Ben Jones --groups cairn-l2 ben"));
        QVERIFY(standIns.entry("ben").contains("\nName=Steam for Ben Jones\n"));
    }

    void theToolsGetAnEnvironmentOfTheirOwn() {
        qputenv("CAIRN_TEST_CALLERS_OWN", "leaked");
        StandIns standIns;
        standIns.setAccounts({});
        QCOMPARE(standIns.run({"create-child", "ada", "1"}), std::nullopt);
        qunsetenv("CAIRN_TEST_CALLERS_OWN");
        const QStringList environment = standIns.environment()
                                            .split('\n', Qt::SkipEmptyParts)
                                            .filter(QRegularExpression("^(?!PWD=|SHLVL=|_=)"));
        QCOMPARE(environment, QStringList({"LC_ALL=C", "PATH=/usr/sbin:/usr/bin"}));
    }

    void aSecondRunChangesNothing() {
        const StandIns standIns;
        standIns.writeEntry("ada", SteamEntry::text("ada", "Ada", steamFor));
        QCOMPARE(standIns.run({"create-child", "ada", "1"}), std::nullopt);
        QCOMPARE(standIns.run({"create-child", "ada", "1", "--full-name", "Ada"}), std::nullopt);
        QCOMPARE(standIns.run({"set-level", "ada", "1"}), std::nullopt);
        QCOMPARE(standIns.calls(), QStringList());
    }

    void aChildWithoutAnEntryGetsOne() {
        const StandIns standIns;
        QCOMPARE(standIns.run({"set-level", "ben", "2"}), std::nullopt);
        QCOMPARE(standIns.calls(), QStringList({standIns.restorecon("ben")}));
        QCOMPARE(standIns.entry("ben"), SteamEntry::text("ben", "ben", steamFor));
    }

    void anOldEntryIsWrittenAgain() {
        const StandIns standIns;
        standIns.writeEntry("ada", "[Desktop Entry]\nExec=/usr/local/bin/cairn-steam-for ada\n");
        QCOMPARE(standIns.run({"create-child", "ada", "1"}), std::nullopt);
        QCOMPARE(standIns.calls(), QStringList({standIns.restorecon("ada")}));
        QCOMPARE(standIns.entry("ada"), SteamEntry::text("ada", "Ada", steamFor));
    }

    void withoutRestoreconTheEntryIsStillWritten() {
        const StandIns standIns;
        FamilyCommand::Setup setup = standIns.setup();
        setup.restorecon = standIns.applications() + "/no-restorecon-here";
        QCOMPARE(FamilyCommand(setup).run({"set-level", "ben", "2"}).refusal, std::nullopt);
        QCOMPARE(standIns.calls(), QStringList());
        QCOMPARE(standIns.entry("ben"), SteamEntry::text("ben", "ben", steamFor));
    }

    void anUnlockedPasswordIsLocked() {
        StandIns standIns;
        standIns.setLocked(false);
        standIns.writeEntry("ada", SteamEntry::text("ada", "Ada", steamFor));
        QCOMPARE(standIns.run({"create-child", "ada", "1"}), std::nullopt);
        QCOMPARE(standIns.calls(), QStringList({"passwd --lock ada"}));
    }

    void aPasswordThatCannotBeReadIsLocked() {
        const StandIns standIns;
        standIns.writeEntry("ada", SteamEntry::text("ada", "Ada", steamFor));
        FamilyCommand::Setup setup = standIns.setup();
        setup.passwordLocked = [](const QString&) -> std::optional<bool> { return std::nullopt; };
        QCOMPARE(FamilyCommand(setup).run({"create-child", "ada", "1"}).refusal, std::nullopt);
        QCOMPARE(standIns.calls(), QStringList({"passwd --lock ada"}));
    }

    void aNewFullName() {
        const StandIns standIns;
        QCOMPARE(standIns.run({"create-child", "ada", "1", "--full-name", "Ada Jones"}),
                 std::nullopt);
        QCOMPARE(standIns.calls(),
                 QStringList({"usermod --comment Ada Jones ada", standIns.restorecon("ada")}));
        QVERIFY(standIns.entry("ada").contains("\nName=Steam for Ada Jones\n"));
    }

    // Added before removed, whichever subcommand asks.
    void aNewLevel_data() {
        QTest::addColumn<QString>("subcommand");
        QTest::newRow("set-level") << "set-level";
        QTest::newRow("create-child again") << "create-child";
    }

    void aNewLevel() {
        QFETCH(const QString, subcommand);
        const StandIns standIns;
        QCOMPARE(standIns.run({subcommand, "ada", "2"}), std::nullopt);
        QCOMPARE(standIns.calls(),
                 QStringList({"gpasswd --add ada cairn-l2", "gpasswd --delete ada cairn-l1",
                              standIns.restorecon("ada")}));
    }

    void aChildInTwoLevelsIsLeftInOne_data() {
        QTest::addColumn<QString>("level");
        QTest::addColumn<QString>("dropped");
        QTest::newRow("keeps 1") << "1" << "cairn-l2";
        QTest::newRow("keeps 2") << "2" << "cairn-l1";
    }

    void aChildInTwoLevelsIsLeftInOne() {
        QFETCH(const QString, level);
        QFETCH(const QString, dropped);
        StandIns standIns;
        standIns.setAccounts(familyWith("ada", {"ada", "cairn-l1", "cairn-l2"}));
        standIns.writeEntry("ada", SteamEntry::text("ada", "ada", steamFor));
        QCOMPARE(standIns.run({"set-level", "ada", level}), std::nullopt);
        QCOMPARE(standIns.calls(), QStringList({"gpasswd --delete ada " + dropped}));
    }

    void anOlderChildMayComeBackToTheKiosk() {
        StandIns standIns;
        standIns.setAccounts(familyWith("ben", {"ben", "cairn-l3"}));
        standIns.writeEntry("ben", SteamEntry::text("ben", "ben", steamFor));
        QCOMPARE(standIns.run({"set-level", "ben", "2"}), std::nullopt);
        QCOMPARE(standIns.calls(),
                 QStringList({"gpasswd --add ben cairn-l2", "gpasswd --delete ben cairn-l3"}));
    }

    void accountsCairnDidNotMakeAreRefused_data() {
        QTest::addColumn<QString>("login");
        QTest::newRow("a Guardian") << "gina";
        QTest::newRow("a Guardian without wheel") << "dad";
        QTest::newRow("another administrator") << "bazzite";
        QTest::newRow("a plain account") << "lodger";
        QTest::newRow("a system account in a level group") << "daemon";
        QTest::newRow("an administrator in a level group") << "admin-child";
        QTest::newRow("a Guardian in a level group") << "guardian-child";
    }

    void accountsCairnDidNotMakeAreRefused() {
        QFETCH(const QString, login);
        const StandIns standIns;
        QCOMPARE(standIns.run({"create-child", login, "1"}), std::optional(Refusal::Taken));
        QCOMPARE(standIns.run({"create-child", login, "2", "--full-name", "Ada"}),
                 std::optional(Refusal::Taken));
        QCOMPARE(standIns.run({"set-level", login, "1"}), std::optional(Refusal::NotAChild));
        QCOMPARE(standIns.calls(), QStringList());
        QVERIFY(!QDir(standIns.applications()).exists());
    }

    void setLevelNeedsAChildThatIsThere() {
        StandIns standIns;
        standIns.setAccounts({});
        QCOMPARE(standIns.run({"set-level", "ada", "1"}), std::optional(Refusal::NotAChild));
        QCOMPARE(standIns.calls(), QStringList());
    }

    void aToolThatFailsStopsTheRest_data() {
        QTest::addColumn<QString>("tool");
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QStringList>("calls");
        QTest::newRow("useradd") << "useradd" << QStringList{"create-child", "ben", "1"}
                                 << QStringList{"useradd --create-home --groups cairn-l1 ben"};
        QTest::newRow("passwd") << "passwd" << QStringList{"create-child", "ben", "1"}
                                << QStringList{"useradd --create-home --groups cairn-l1 ben",
                                               "passwd --lock ben"};
        QTest::newRow("gpasswd") << "gpasswd" << QStringList{"set-level", "ada", "2"}
                                 << QStringList{"gpasswd --add ada cairn-l2"};
        QTest::newRow("usermod") << "usermod"
                                 << QStringList{"create-child", "ada", "2", "--full-name", "A"}
                                 << QStringList{"usermod --comment A ada"};
    }

    void aToolThatFailsStopsTheRest() {
        QFETCH(const QString, tool);
        QFETCH(const QStringList, arguments);
        QFETCH(const QStringList, calls);
        StandIns standIns;
        if (arguments.at(1) == "ben") {
            standIns.setAccounts({});
        }
        standIns.fail(tool);
        QCOMPARE(standIns.run(arguments), std::optional(Refusal::Failed));
        QCOMPARE(standIns.calls(), calls);
        QVERIFY(!QDir(standIns.applications()).exists());
    }

    void aFailedRelabelIsAFailure() {
        const StandIns standIns;
        standIns.fail("restorecon");
        QCOMPARE(standIns.run({"set-level", "ben", "2"}), std::optional(Refusal::Failed));
    }

    void whatTheJournalIsToldHoldsOnlyCheckedValues() {
        const StandIns standIns;
        QCOMPARE(FamilyCommand(standIns.setup()).run({"set-level", "ada", "2"}).asked,
                 QStringLiteral("set-level ada at level 2"));
        QCOMPARE(FamilyCommand(standIns.setup())
                     .run({"create-child", "ada", "1", "--full-name", "Ada"})
                     .asked,
                 QStringLiteral("create-child ada at level 1"));
        QCOMPARE(FamilyCommand(standIns.setup()).run({"create-child", "x;id", "1"}).asked,
                 QString());
    }
};

} // namespace

QTEST_GUILESS_MAIN(FamilyCommandTest)
#include "tst_familycommand.moc"
