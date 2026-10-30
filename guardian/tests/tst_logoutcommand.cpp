// SPDX-License-Identifier: Apache-2.0
#include "LogOutCommand.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QTemporaryDir>
#include <QTest>

namespace {

// Only ada and ben are children's names.
QHash<QString, Account> family() {
    return {
        {"ada", {.uid = 1002, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = {}}},
        {"ben", {.uid = 1003, .groups = {"ben", "cairn-l2"}, .login = "ben", .fullName = {}}},
        {"gina",
         {.uid = 1001,
          .groups = {"gina", "wheel", "cairn-guardian"},
          .login = "gina",
          .fullName = {}}},
        {"admin-child",
         {.uid = 1007,
          .groups = {"admin-child", "wheel", "cairn-l1"},
          .login = "admin-child",
          .fullName = {}}},
    };
}

class LogOutCommandTest : public QObject {
    Q_OBJECT

    LogOutCommand::Outcome run(const QStringList& arguments) {
        LogOutCommand::Setup setup;
        setup.euid = m_euid;
        setup.pkexecUid = m_pkexecUid;
        const QHash<QString, Account> accounts = family();
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
        setup.lookUpHome = [](unsigned int uid) -> std::optional<QString> {
            return QStringLiteral("/var/home/user%1").arg(uid);
        };
        setup.systemdRun = path("systemd-run");
        setup.visitHelper = path("cairn-steam-visit");
        return LogOutCommand(setup).run(arguments,
                                        [this](const QString& line) { m_said.append(line); });
    }

    QString path(const QString& name) const {
        return QDir(m_temporary->path()).filePath(name);
    }

    void give(const QString& name, const QByteArray& text) const {
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(text);
    }

    QStringList lines(const QString& name) const {
        QFile file(path(name));
        return file.open(QIODevice::ReadOnly)
                   ? QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)
                   : QStringList();
    }

    std::unique_ptr<QTemporaryDir> m_temporary;
    QStringList m_said;
    unsigned int m_euid = 0;
    std::optional<QString> m_pkexecUid;

private slots:
    void init() {
        m_temporary = std::make_unique<QTemporaryDir>();
        QVERIFY(m_temporary->isValid());
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/systemd-run"), path("systemd-run")));
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/log-out-helper"), path("cairn-steam-visit")));
        m_said.clear();
        m_euid = 0;
        m_pkexecUid = QStringLiteral("1001");
    }

    // The helper runs as a service of its own, named for the child, and
    // only ever logs out: no copy, no inhibitor.
    void logsTheChildOutAsASystemJob() {
        const LogOutCommand::Outcome outcome = run({"log-out", "ada"});
        QVERIFY(!outcome.refusal.has_value());
        QCOMPARE(lines("systemd-run-calls"),
                 QStringList({"--quiet --collect --wait --pipe --no-ask-password "
                              "--service-type=exec --expand-environment=no "
                              "--unit=cairn-log-out-1002 "
                              "--description=Cairn logs a child out for a grown-up -- " +
                              path("cairn-steam-visit") + " --log-out ada"}));
        QCOMPARE(lines("helper-calls"), QStringList({"--log-out ada"}));
        QCOMPARE(m_said, QStringList({"logging-out ada", "logged-out ada"}));
        QCOMPARE(outcome.asked, QStringLiteral("log-out ada"));
        QCOMPARE(outcome.ended, QStringLiteral("logged-out ada"));
        const QString environment = QString::fromUtf8([this] {
            QFile file(path("systemd-run-environment"));
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        }());
        QVERIFY(!environment.contains("PKEXEC_UID="));
    }

    // The helper's words only, each with the login from the argument.
    void passesOnlyTheHelpersWords() {
        give("lines", "logging-out\nlogged-out ben\nhello\nlogging-out\nlogged-out\nfinished\n"
                      "logged-out\n");
        run({"log-out", "ada"});
        QCOMPARE(m_said, QStringList({"logging-out ada", "logging-out ada", "logged-out ada"}));
    }

    void holds_data() {
        QTest::addColumn<QByteArray>("lines");
        QTest::addColumn<QString>("held");
        QTest::newRow("stuck") << QByteArray("logging-out\nrefused stuck\n") << "stuck";
        QTest::newRow("still closing")
            << QByteArray("logging-out\nrefused logged-in\n") << "logged-in";
        QTest::newRow("stopped") << QByteArray("logging-out\nrefused stopped\n") << "stopped";
        QTest::newRow("a visit") << QByteArray("refused busy\n") << "busy";
        QTest::newRow("a copy") << QByteArray("refused copying\n") << "copying";
        QTest::newRow("another logout") << QByteArray("refused logging-out\n") << "logging-out";
        QTest::newRow("not set up") << QByteArray("refused not-set-up\n") << "not-set-up";
        QTest::newRow("a mistake") << QByteArray("refused not-for-pkexec\n") << "failed";
        QTest::newRow("no last word") << QByteArray("logging-out\n") << "failed";
        QTest::newRow("garbage") << QByteArray("hello\n") << "failed";
    }

    // A logout that did not finish says why in one line, and still ends.
    void holds() {
        QFETCH(const QByteArray, lines);
        QFETCH(const QString, held);
        give("lines", lines);
        give("status", "1");
        const LogOutCommand::Outcome outcome = run({"log-out", "ada"});
        QVERIFY(!outcome.refusal.has_value());
        QCOMPARE(m_said.constLast(), QStringLiteral("held ada ") + held);
        QCOMPARE(outcome.ended, QStringLiteral("held ada ") + held);
    }

    // systemd-run says nothing: the unit's name is taken.
    void notStarted() {
        give("fail-start", "");
        run({"log-out", "ada"});
        QCOMPARE(m_said, QStringList({"held ada not-started"}));
        QVERIFY(lines("helper-calls").isEmpty());
    }

    void refusesBeforeAnything_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QString>("word");
        QTest::newRow("no child") << QStringList{"log-out"} << "usage";
        QTest::newRow("two children") << QStringList{"log-out", "ada", "ben"} << "usage";
        QTest::newRow("an option") << QStringList{"log-out", "--all"} << "bad-name";
        QTest::newRow("a path") << QStringList{"log-out", "../ada"} << "bad-name";
        QTest::newRow("unknown") << QStringList{"log-out", "cy"} << "not-a-child";
        QTest::newRow("a grown-up") << QStringList{"log-out", "gina"} << "not-a-child";
        QTest::newRow("wheel and a level")
            << QStringList{"log-out", "admin-child"} << "not-a-child";
    }

    void refusesBeforeAnything() {
        QFETCH(const QStringList, arguments);
        QFETCH(const QString, word);
        const LogOutCommand::Outcome outcome = run(arguments);
        QCOMPARE(refusalWord(outcome.refusal.value_or(Refusal::Failed)), word);
        QVERIFY(m_said.isEmpty());
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
    }

    void refusesAnyoneButAGrownUp() {
        m_pkexecUid = QStringLiteral("1002");
        QCOMPARE(run({"log-out", "ada"}).refusal, std::optional(Refusal::NotAGrownUp));
        m_euid = 1001;
        QCOMPARE(run({"log-out", "ada"}).refusal, std::optional(Refusal::NotRoot));
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(LogOutCommandTest)
#include "tst_logoutcommand.moc"
