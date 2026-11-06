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
        lock = rules().lockFrom(lockScreenLine(lockLine("ada", "kde-fingerprint"), "1001"));
        QCOMPARE(lock->account.login, QStringLiteral("ada"));
        QVERIFY(rules().lockFrom(lockScreenLine(lockLine("ada"), "0")).has_value());
        QVERIFY(
            rules()
                .lockFrom(lockScreenLine(lockLine("ada"), "1001", "/usr/lib/kscreenlocker_greet"))
                .has_value());
    }

    // A child can write this line, from the lock screen's own program too
    // (LD_PRELOAD), so it counts only for the child's own account.
    void aForgedLockLine_data() {
        QTest::addColumn<QJsonObject>("entry");
        QTest::newRow("a Guardian's, from a child") << lockScreenLine(lockLine("guardian"), "1002");
        QTest::newRow("through logger")
            << lockScreenLine(lockLine("guardian"), "1000", "/usr/bin/logger");
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
