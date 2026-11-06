// SPDX-License-Identifier: Apache-2.0
#include "JournalTries.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTest>

#include <vector>

namespace {

// A family of a Guardian, two children and the installer's account, in
// place of the system's accounts.
const std::vector<Account>& family() {
    static const std::vector<Account> accounts{
        {.uid = 1000,
         .groups = {"guardian", "wheel", "cairn-guardian"},
         .login = "guardian",
         .fullName = "Guardian"},
        {.uid = 1001, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"},
        {.uid = 1002, .groups = {"ben", "cairn-l3"}, .login = "ben", .fullName = "Ben"},
        {.uid = 999, .groups = {"bazzite", "wheel"}, .login = "bazzite", .fullName = "cairn"},
        // A Guardian also in a level group, whom the login screen still
        // asks for a password (ADR-0023).
        {.uid = 1003,
         .groups = {"cora", "cairn-l2", "cairn-guardian"},
         .login = "cora",
         .fullName = "Cora"},
        // An account in a level group and wheel but not cairn-guardian,
        // whom the login screen also asks for a password.
        {.uid = 1004, .groups = {"dana", "cairn-l1", "wheel"}, .login = "dana", .fullName = "Dana"},
    };
    return accounts;
}

JournalTries rules() {
    JournalTries::Lookups lookups;
    lookups.byName = [](const QString& name) -> std::optional<Account> {
        for (const Account& account : family()) {
            if (account.login == name) {
                return account;
            }
        }
        return std::nullopt;
    };
    lookups.byUid = [](unsigned int uid) -> std::optional<Account> {
        for (const Account& account : family()) {
            if (account.uid == uid) {
                return account;
            }
        }
        return std::nullopt;
    };
    return JournalTries(lookups);
}

// A failed USER_AUTH record as journald splits it, from the dev PC's and
// cairn-min's journals.
QJsonObject failedAuth(const QString& account, const QString& program,
                       const QString& op = "PAM:authentication",
                       const QString& loginUid = "4294967295") {
    return {{"__REALTIME_TIMESTAMP", "1793312010102890"},
            {"_TRANSPORT", "audit"},
            {"_AUDIT_TYPE", "1100"},
            {"_PID", "1801"},
            {"_UID", "0"},
            {"_AUDIT_LOGINUID", loginUid},
            {"AUDIT_FIELD_OP", op},
            {"AUDIT_FIELD_ACCT", account},
            {"AUDIT_FIELD_EXE", program},
            {"AUDIT_FIELD_RES", "failed"}};
}

QJsonObject auditLock(const QString& uid, const QString& program = "/usr/bin/sudo") {
    return {{"__REALTIME_TIMESTAMP", "1793312010102890"},
            {"_TRANSPORT", "audit"},
            {"_AUDIT_TYPE", "2100"},
            {"_PID", "1801"},
            {"AUDIT_FIELD_OP", "pam_faillock"},
            {"AUDIT_FIELD_SUID", uid},
            {"AUDIT_FIELD_EXE", program},
            {"AUDIT_FIELD_RES", "success"}};
}

// faillock's line from the lock screen, sent by a process of `senderUid`.
QJsonObject lockScreenLine(const QString& message, const QString& senderUid,
                           const QString& program = "/usr/libexec/kscreenlocker_greet") {
    return {{"__REALTIME_TIMESTAMP", "1793312010102890"},
            {"_TRANSPORT", "syslog"},
            {"SYSLOG_FACILITY", "10"},
            {"_PID", "4242"},
            {"_UID", senderUid},
            {"_EXE", program},
            {"MESSAGE", message}};
}

QString lockLine(const QString& login, const QString& service = "kde") {
    return "pam_faillock(" + service + ":auth): Consecutive login failures for user " + login +
           " account temporarily locked";
}

class JournalTriesTest : public QObject {
    Q_OBJECT

private slots:
    void eachPlace_data() {
        QTest::addColumn<QString>("program");
        QTest::addColumn<QString>("op");
        QTest::addColumn<int>("place");
        QTest::newRow("sddm") << "/usr/libexec/sddm-helper" << "PAM:authentication"
                              << int(TryPlace::LoginScreen);
        // Fedora 44's own login screen, which a rebase can bring.
        QTest::newRow("plasma login") << "/usr/libexec/plasmalogin-helper"
                                      << "PAM:authentication" << int(TryPlace::LoginScreen);
        QTest::newRow("unix_chkpwd")
            << "/usr/bin/unix_chkpwd" << "PAM:unix_chkpwd" << int(TryPlace::OwnLogin);
        QTest::newRow("sudo") << "/usr/bin/sudo" << "PAM:authentication" << int(TryPlace::Sudo);
        QTest::newRow("su") << "/usr/bin/su" << "PAM:authentication" << int(TryPlace::Su);
        QTest::newRow("polkit") << "/usr/lib/polkit-1/polkit-agent-helper-1"
                                << "PAM:authentication" << int(TryPlace::PasswordBox);
        QTest::newRow("console") << "/usr/bin/login" << "PAM:authentication"
                                 << int(TryPlace::Console);
        QTest::newRow("unknown") << "/usr/sbin/sshd" << "PAM:authentication"
                                 << int(TryPlace::Other);
    }

    void eachPlace() {
        QFETCH(QString, program);
        QFETCH(QString, op);
        QFETCH(int, place);
        const auto found = rules().tryFrom(failedAuth("guardian", program, op));
        QVERIFY(found.has_value());
        QCOMPARE(int(found->place), place);
        QCOMPARE(found->time, 1793312010102890);
        QCOMPARE(found->pid, 1801);
        QCOMPARE(found->account->login, QStringLiteral("guardian"));
        QVERIFY(!found->from.has_value());
    }

    // Another program is named by its file name, so a way in nobody knew
    // about still shows.
    void anotherProgramIsNamed() {
        const auto found = rules().tryFrom(failedAuth("guardian", "/usr/sbin/sshd"));
        QCOMPARE(found->program, QStringLiteral("sshd"));
        QCOMPARE(rules().tryFrom(failedAuth("guardian", "/usr/bin/sudo"))->program, QString());
    }

    // A name that is not plain is not shown at all.
    void anOddProgramNameIsNotShown_data() {
        QTest::addColumn<QString>("program");
        QTest::newRow("markup") << "/tmp/<b>x</b>";
        QTest::newRow("space") << "/tmp/two words";
        QTest::newRow("bidi") << QString("/tmp/abc") + QChar(0x202E) + "exe";
        QTest::newRow("newline") << "/tmp/a\nb";
        QTest::newRow("too long") << "/tmp/" + QString(65, 'a');
        QTest::newRow("none") << "";
    }

    void anOddProgramNameIsNotShown() {
        QFETCH(QString, program);
        const auto found = rules().tryFrom(failedAuth("guardian", program));
        QVERIFY(found.has_value());
        QCOMPARE(int(found->place), int(TryPlace::Other));
        QCOMPARE(found->program, QString());
    }

    // su from a child's login: the login it came from is named.
    void theLoginItCameFrom() {
        const auto found =
            rules().tryFrom(failedAuth("guardian", "/usr/bin/su", "PAM:authentication", "1002"));
        QCOMPARE(found->from->login, QStringLiteral("ben"));
        // Typed in the account's own login: no "from".
        QVERIFY(!rules()
                     .tryFrom(failedAuth("guardian", "/usr/bin/sudo", "PAM:authentication", "1000"))
                     ->from.has_value());
        // A login the computer no longer has keeps only its number.
        const auto gone =
            rules().tryFrom(failedAuth("guardian", "/usr/bin/su", "PAM:authentication", "1500"));
        QCOMPARE(gone->from->uid, 1500U);
        QCOMPARE(gone->from->login, QString());
    }

    // Names typed at su are anything a child likes. None is ever kept as
    // text: a name that is no account is no account.
    void aNameThatIsNoAccount_data() {
        QTest::addColumn<QJsonValue>("name");
        QTest::newRow("unknown") << QJsonValue("nobody-here");
        QTest::newRow("unknown to PAM") << QJsonValue("(unknown)");
        QTest::newRow("question mark") << QJsonValue("?");
        QTest::newRow("spaces") << QJsonValue("guardian x");
        QTest::newRow("newline") << QJsonValue("guardian\nada");
        QTest::newRow("markup") << QJsonValue("<b>guardian</b>");
        QTest::newRow("bidi") << QJsonValue(QString("ada") + QChar(0x202E));
        QTest::newRow("long") << QJsonValue(QString(300, 'a'));
        QTest::newRow("empty") << QJsonValue("");
        // journalctl's form for a field that is not UTF-8.
        QTest::newRow("bytes") << QJsonValue(QJsonArray{103, 255, 0});
        QTest::newRow("missing") << QJsonValue(QJsonValue::Undefined);
    }

    void aNameThatIsNoAccount() {
        QFETCH(QJsonValue, name);
        QJsonObject entry = failedAuth("guardian", "/usr/bin/su");
        if (name.isUndefined()) {
            entry.remove("AUDIT_FIELD_ACCT");
        } else {
            entry.insert("AUDIT_FIELD_ACCT", name);
        }
        const auto found = rules().tryFrom(entry);
        QVERIFY(found.has_value());
        QVERIFY(!found->account.has_value());
    }

    // Only a failed audit password check is a try. Anything a child can
    // write, such as a syslog line through logger, is not.
    void notATry_data() {
        QTest::addColumn<QJsonObject>("entry");
        QJsonObject entry = failedAuth("guardian", "/usr/bin/sudo");
        entry.insert("AUDIT_FIELD_RES", "success");
        QTest::newRow("succeeded") << entry;
        entry = failedAuth("guardian", "/usr/bin/sudo");
        entry.insert("_TRANSPORT", "syslog");
        QTest::newRow("syslog") << entry;
        entry = failedAuth("guardian", "/usr/bin/sudo");
        entry.insert("_TRANSPORT", "journal");
        QTest::newRow("journal") << entry;
        entry = failedAuth("guardian", "/usr/bin/sudo");
        entry.insert("_AUDIT_TYPE", "1105");
        QTest::newRow("another record") << entry;
        QTest::newRow("key check") << failedAuth("guardian", "/usr/sbin/sshd", "pubkey");
        QTest::newRow("credentials")
            << failedAuth("guardian", "/usr/lib/systemd/systemd-executor", "setcred");
        entry = failedAuth("guardian", "/usr/bin/sudo");
        entry.insert("__REALTIME_TIMESTAMP", "17933x");
        QTest::newRow("odd time") << entry;
        entry.remove("__REALTIME_TIMESTAMP");
        QTest::newRow("no time") << entry;
        entry = failedAuth("guardian", "/usr/bin/sudo");
        entry.insert("_AUDIT_TYPE", QJsonArray{"1100", "1100"});
        QTest::newRow("twice") << entry;
        QTest::newRow("a child's logger line")
            << lockScreenLine("pam_unix(sudo:auth): authentication failure; logname=ben uid=1002 "
                              "euid=0 tty=/dev/pts/0 "
                              "ruser=ben rhost=  user=guardian",
                              "1002", "/usr/bin/logger");
    }

    void notATry() {
        QFETCH(QJsonObject, entry);
        QVERIFY(!rules().tryFrom(entry).has_value());
    }

    void faillocksAuditLock() {
        const auto lock = rules().lockFrom(auditLock("1000"));
        QVERIFY(lock.has_value());
        QCOMPARE(lock->account.login, QStringLiteral("guardian"));
        QCOMPARE(int(lock->place), int(TryPlace::Sudo));
        QCOMPARE(lock->pid, 1801);
        QVERIFY(!rules().lockFrom(auditLock("4242")).has_value());
        QVERIFY(!rules().lockFrom(auditLock("x")).has_value());
        QJsonObject noUid = auditLock("1000");
        noUid.remove("AUDIT_FIELD_SUID");
        QVERIFY(!rules().lockFrom(noUid).has_value());
    }

    // The lock screen runs as the person: its line counts for that person,
    // with the password stack or the fingerprint one.
    void theLockScreensLock() {
        auto lock = rules().lockFrom(lockScreenLine(lockLine("guardian"), "1000"));
        QVERIFY(lock.has_value());
        QCOMPARE(lock->account.uid, 1000U);
        QCOMPARE(int(lock->place), int(TryPlace::OwnLogin));
        lock = rules().lockFrom(lockScreenLine(lockLine("ben", "kde-fingerprint"), "1002"));
        QCOMPARE(lock->account.login, QStringLiteral("ben"));
        QVERIFY(rules().lockFrom(lockScreenLine(lockLine("ben"), "0")).has_value());
        QVERIFY(
            rules()
                .lockFrom(lockScreenLine(lockLine("ben"), "1002", "/usr/lib/kscreenlocker_greet"))
                .has_value());
    }

    // Plasma 6.8 runs the lock screen's PAM in kscreenlocker_worker, so
    // faillock's line comes from there (cairn-min, 2026-11-05).
    void theLockScreensWorker() {
        const QString worker = QStringLiteral("/usr/libexec/kscreenlocker_worker");
        auto lock = rules().lockFrom(lockScreenLine(lockLine("guardian"), "1000", worker));
        QVERIFY(lock.has_value());
        QCOMPARE(lock->account.uid, 1000U);
        QCOMPARE(int(lock->place), int(TryPlace::OwnLogin));
        lock =
            rules().lockFrom(lockScreenLine(lockLine("cora", "kde-fingerprint"), "1003", worker));
        QVERIFY(lock.has_value());
    }

    // The uid rule on its own: the lock screen's real program, and a
    // line about an account other than the one it ran as, is dropped,
    // whichever program it was. Only root, or the account itself, is
    // believed.
    void theUidRule_data() {
        QTest::addColumn<QString>("program");
        QTest::addColumn<QString>("login");
        QTest::addColumn<QString>("sender");
        QTest::addColumn<bool>("believed");
        for (const QString& program : {QStringLiteral("/usr/libexec/kscreenlocker_worker"),
                                       QStringLiteral("/usr/libexec/kscreenlocker_greet")}) {
            const QByteArray name = program.section('/', -1).toUtf8();
            QTest::newRow(name + ", own account") << program << "ben" << "1002" << true;
            QTest::newRow(name + ", root") << program << "ben" << "0" << true;
            QTest::newRow(name + ", a Guardian's, from a child")
                << program << "guardian" << "1002" << false;
            QTest::newRow(name + ", a child's, from another child")
                << program << "ada" << "1002" << false;
            QTest::newRow(name + ", a child's, from a Guardian")
                << program << "ben" << "1000" << false;
            QTest::newRow(name + ", from an account that is gone")
                << program << "guardian" << "4242" << false;
        }
    }

    void theUidRule() {
        QFETCH(QString, program);
        QFETCH(QString, login);
        QFETCH(QString, sender);
        QFETCH(bool, believed);
        QCOMPARE(rules().lockFrom(lockScreenLine(lockLine(login), sender, program)).has_value(),
                 believed);
    }

    // A child can write this line, from the lock screen's own program too
    // (LD_PRELOAD), so it counts only for the child's own account.
    void aForgedLockLine_data() {
        QTest::addColumn<QJsonObject>("entry");
        QTest::newRow("a Guardian's, from a child") << lockScreenLine(lockLine("guardian"), "1002");
        QTest::newRow("through logger")
            << lockScreenLine(lockLine("guardian"), "1000", "/usr/bin/logger");
        // A program of the child's own, named as the lock screen's.
        QTest::newRow("a worker in the child's home")
            << lockScreenLine(lockLine("ben"), "1002", "/home/ben/kscreenlocker_worker");
        QTest::newRow("a worker in /tmp")
            << lockScreenLine(lockLine("ben"), "1002", "/tmp/usr/libexec/kscreenlocker_worker");
        QTest::newRow("the worker's name alone")
            << lockScreenLine(lockLine("ben"), "1002", "kscreenlocker_worker");
        QJsonObject entry = lockScreenLine(lockLine("guardian"), "1000");
        entry.remove("_EXE");
        QTest::newRow("no program") << entry;
        entry = lockScreenLine(lockLine("guardian"), "1000");
        entry.insert("SYSLOG_FACILITY", "1");
        QTest::newRow("user facility") << entry;
        entry = lockScreenLine(lockLine("guardian"), "1000");
        entry.insert("_TRANSPORT", "journal");
        QTest::newRow("native") << entry;
        entry = lockScreenLine(lockLine("guardian"), "1000");
        entry.remove("_UID");
        QTest::newRow("no sender") << entry;
        QTest::newRow("another service") << lockScreenLine(lockLine("guardian", "sudo"), "1000");
        QTest::newRow("no such account") << lockScreenLine(lockLine("nobody"), "0");
        QTest::newRow("two lines") << lockScreenLine(lockLine("guardian") + "\nmore", "1000");
        QTest::newRow("a name with a newline") << lockScreenLine(lockLine("guar\ndian"), "0");
        QTest::newRow("words around") << lockScreenLine("x " + lockLine("guardian"), "1000");
    }

    void aForgedLockLine() {
        QFETCH(QJsonObject, entry);
        QVERIFY(!rules().lockFrom(entry).has_value());
    }

    // A level 1 or 2 child's kiosk has no lock screen, and faillock never
    // stops their login, so a lock screen line about them locks nothing
    // that matters. In a user namespace a child can run their own program
    // at the lock screen's path and write faillock's line about themselves
    // (cairn-min, 2026-11-06), so that line is dropped, from root too, and
    // so is their own try through unix_chkpwd. A Guardian or wheel member
    // in a level group, and a level 3 child, still count.
    void aChildWhoseLockScreenCannotLock_data() {
        QTest::addColumn<QString>("login");
        QTest::addColumn<QString>("sender");
        QTest::addColumn<bool>("believed");
        QTest::newRow("level 1, own") << "ada" << "1001" << false;
        QTest::newRow("level 1, from root") << "ada" << "0" << false;
        QTest::newRow("level 3, own") << "ben" << "1002" << true;
        QTest::newRow("a Guardian at level 2, own") << "cora" << "1003" << true;
        QTest::newRow("wheel at level 1, own") << "dana" << "1004" << true;
        QTest::newRow("a Guardian, own") << "guardian" << "1000" << true;
    }

    void aChildWhoseLockScreenCannotLock() {
        QFETCH(QString, login);
        QFETCH(QString, sender);
        QFETCH(bool, believed);
        const QString worker = QStringLiteral("/usr/libexec/kscreenlocker_worker");
        for (const QString service : {"kde", "kde-fingerprint"}) {
            QCOMPARE(rules()
                         .lockFrom(lockScreenLine(lockLine(login, service), sender, worker))
                         .has_value(),
                     believed);
        }
        const auto chkpwd =
            rules().tryFrom(failedAuth(login, "/usr/bin/unix_chkpwd", "PAM:unix_chkpwd", sender));
        QCOMPARE(chkpwd.has_value(), believed);
    }

    // At any other prompt a level 1 or 2 child's tries count: a wrong
    // password at su for them is someone trying their account, and
    // faillock's own audit lock is real. unix_chkpwd in either folder is
    // the lock screen's check.
    void aChildsTriesElsewhere() {
        QVERIFY(rules().tryFrom(failedAuth("ada", "/usr/bin/su")).has_value());
        QVERIFY(rules().tryFrom(failedAuth("ada", "/usr/bin/sudo")).has_value());
        QVERIFY(!rules()
                     .tryFrom(failedAuth("ada", "/usr/sbin/unix_chkpwd", "PAM:unix_chkpwd"))
                     .has_value());
        QVERIFY(rules().lockFrom(auditLock("1001", "/usr/bin/su")).has_value());
    }

    // A right password, and the account part after it, which faillock's
    // count goes by. Never one that failed, nor sshd's key check, nor one
    // for a name that is no account.
    void aRightPassword() {
        QJsonObject entry = failedAuth("guardian", "/usr/libexec/sddm-helper");
        entry.insert("AUDIT_FIELD_RES", "success");
        auto good = rules().goodFrom(entry);
        QVERIFY(good.has_value());
        QCOMPARE(good->account.uid, 1000U);
        QCOMPARE(good->pid, 1801);
        QVERIFY(!good->accountPart);
        QVERIFY(!rules().tryFrom(entry).has_value());
        entry.insert("_AUDIT_TYPE", "1101");
        entry.insert("AUDIT_FIELD_OP", "PAM:accounting");
        good = rules().goodFrom(entry);
        QVERIFY(good.has_value());
        QVERIFY(good->accountPart);
        QJsonObject failed = entry;
        failed.insert("AUDIT_FIELD_RES", "failed");
        QVERIFY(!rules().goodFrom(failed).has_value());
        QVERIFY(!rules().goodFrom(failedAuth("guardian", "/usr/bin/sudo")).has_value());
        QJsonObject key = failedAuth("guardian", "/usr/sbin/sshd", "PAM:authentication");
        key.insert("AUDIT_FIELD_RES", "success");
        key.insert("AUDIT_FIELD_OP", "pubkey");
        QVERIFY(!rules().goodFrom(key).has_value());
        QJsonObject nobody = entry;
        nobody.insert("AUDIT_FIELD_ACCT", "nobody");
        QVERIFY(!rules().goodFrom(nobody).has_value());
        QJsonObject syslog = entry;
        syslog.insert("_TRANSPORT", "syslog");
        QVERIFY(!rules().goodFrom(syslog).has_value());
    }

    // The login screen lets a level 1 or 2 child in with no password, and
    // faillock's part never runs, so that login is no right password. At
    // any other prompt their password was checked, and it is one.
    void aChildsLoginWithoutAPassword() {
        for (const QString op : {"PAM:authentication", "PAM:accounting"}) {
            const bool accountPart = op == QLatin1String("PAM:accounting");
            const auto good = [&](const QString& account, const QString& program) {
                QJsonObject entry = failedAuth(account, program, op);
                entry.insert("AUDIT_FIELD_RES", "success");
                entry.insert("_AUDIT_TYPE", accountPart ? "1101" : "1100");
                return rules().goodFrom(entry);
            };
            QVERIFY(!good("ada", "/usr/libexec/sddm-helper").has_value());
            QVERIFY(!good("ada", "/usr/libexec/plasmalogin-helper").has_value());
            QVERIFY(good("ada", "/usr/bin/su").has_value());
            QVERIFY(good("ben", "/usr/libexec/sddm-helper").has_value());
            QVERIFY(good("cora", "/usr/libexec/sddm-helper").has_value());
            QVERIFY(good("guardian", "/usr/libexec/sddm-helper").has_value());
        }
    }

    void aLineThatIsNotAnObject() {
        QVERIFY(!JournalTries::parse("[1, 2]").has_value());
        QVERIFY(!JournalTries::parse("{\"a\": ").has_value());
        QVERIFY(!JournalTries::parse("").has_value());
        QVERIFY(JournalTries::parse(R"({"MESSAGE": "x"})").has_value());
    }
};

} // namespace

QTEST_GUILESS_MAIN(JournalTriesTest)
#include "tst_journaltries.moc"
