// SPDX-License-Identifier: Apache-2.0
#include "NewPassword.h"
#include "OwnPasswordCommand.h"
#include "SystemTool.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

std::optional<Account> account(unsigned int uid) {
    switch (uid) {
    case 1000:
        return Account{.uid = 1000,
                       .groups = {"guardian", "wheel", "cairn-guardian"},
                       .login = "guardian",
                       .fullName = "Guardian"};
    case 1001:
        return Account{.uid = 1001, .groups = {"gina", "wheel"}, .login = "gina", .fullName = {}};
    case 1002:
        return Account{.uid = 1002, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = {}};
    case 1003:
        return Account{.uid = 1003, .groups = {"cy", "cairn-l3"}, .login = "cy", .fullName = {}};
    case 1006:
        return Account{
            .uid = 1006, .groups = {"lodger", "users"}, .login = "lodger", .fullName = {}};
    default:
        return std::nullopt;
    }
}

// One call to the tool, as the command made it.
struct ToolCall {
    QString program;
    QStringList arguments;
    QByteArray input;
};

// A Setup with stand-ins: the input the test gives, and a tool that writes
// down what it was asked and answers as `toolWorks` says.
class Run {
public:
    explicit Run(std::optional<QByteArray> input) : m_input(std::move(input)) {}

    OwnPasswordCommand::Setup setup(unsigned int euid, std::optional<QString> pkexecUid) {
        OwnPasswordCommand::Setup setup;
        setup.euid = euid;
        setup.pkexecUid = std::move(pkexecUid);
        setup.lookUpUid = account;
        setup.lookUpHome = [](unsigned int uid) {
            return std::optional(QStringLiteral("/var/home/user%1").arg(uid));
        };
        setup.readInput = [this](qint64 maxBytes) {
            m_asked = maxBytes;
            m_read = true;
            return m_input;
        };
        setup.runTool = [this](const QString& program, const QStringList& arguments,
                               const QByteArray& input) {
            calls.append({.program = program, .arguments = arguments, .input = input});
            return toolWorks;
        };
        return setup;
    }

    OwnPasswordCommand::Outcome run(unsigned int euid, std::optional<QString> pkexecUid,
                                    const QStringList& arguments = {"set-own-password"}) {
        return OwnPasswordCommand(setup(euid, std::move(pkexecUid))).run(arguments);
    }

    bool read() const {
        return m_read;
    }

    qint64 asked() const {
        return m_asked;
    }

    QList<ToolCall> calls;
    bool toolWorks = true;

private:
    std::optional<QByteArray> m_input;
    bool m_read = false;
    qint64 m_asked = 0;
};

class OwnPasswordCommandTest : public QObject {
    Q_OBJECT

private slots:
    // The grown-up pkexec names gets the new password, through chpasswd's
    // standard input, and nowhere else.
    void changesTheCallersOwnPassword() {
        Run run(QByteArrayLiteral("new pass: one\n"));
        const OwnPasswordCommand::Outcome outcome = run.run(0, QStringLiteral("1000"));
        QCOMPARE(outcome.refusal, std::nullopt);
        QCOMPARE(outcome.asked, QStringLiteral("set-own-password guardian"));
        QCOMPARE(run.calls.size(), 1);
        QCOMPARE(run.calls.constFirst().program, QStringLiteral("/usr/sbin/chpasswd"));
        QCOMPARE(run.calls.constFirst().arguments, QStringList());
        QCOMPARE(run.calls.constFirst().input, QByteArrayLiteral("guardian:new pass: one\n"));
    }

    void anAdministratorByWheelAlone() {
        Run run(QByteArrayLiteral("x"));
        QCOMPARE(run.run(0, QStringLiteral("1001")).refusal, std::nullopt);
        QCOMPARE(run.calls.constFirst().input, QByteArrayLiteral("gina:x\n"));
    }

    void acceptsUtf8() {
        Run run(QStringLiteral("Zoë 🙂\n").toUtf8());
        QCOMPARE(run.run(0, QStringLiteral("1000")).refusal, std::nullopt);
        QCOMPARE(run.calls.constFirst().input, QStringLiteral("guardian:Zoë 🙂\n").toUtf8());
    }

    // The password is never part of what goes to the journal.
    void theJournalNeverGetsThePassword() {
        Run run(QByteArrayLiteral("hunter2\n"));
        run.toolWorks = false;
        const OwnPasswordCommand::Outcome outcome = run.run(0, QStringLiteral("1000"));
        QCOMPARE(outcome.refusal, std::optional(Refusal::Failed));
        QVERIFY(!outcome.asked.contains(QStringLiteral("hunter2")));
    }

    // Who may ask, checked before the input is read.
    void refusesBeforeReadingAnything_data() {
        QTest::addColumn<unsigned int>("euid");
        QTest::addColumn<std::optional<QString>>("pkexecUid");
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<Refusal>("refusal");

        const QStringList plain{"set-own-password"};
        QTest::newRow("not root") << 1000U << std::optional<QString>("1000") << plain
                                  << Refusal::NotRoot;
        QTest::newRow("a child") << 0U << std::optional<QString>("1002") << plain
                                 << Refusal::NotAGrownUp;
        QTest::newRow("an older child")
            << 0U << std::optional<QString>("1003") << plain << Refusal::NotAGrownUp;
        QTest::newRow("an account Cairn did not make")
            << 0U << std::optional<QString>("1006") << plain << Refusal::NotAGrownUp;
        QTest::newRow("no such account")
            << 0U << std::optional<QString>("4242") << plain << Refusal::NotAGrownUp;
        QTest::newRow("not a number")
            << 0U << std::optional<QString>("1000; rm") << plain << Refusal::NotAGrownUp;
        // Under sudo no grown-up is named, so there is no "own" account.
        QTest::newRow("sudo") << 0U << std::optional<QString>() << plain << Refusal::NotAGrownUp;
        // No login, or anything else, may follow: only one's own password.
        QTest::newRow("a login after it")
            << 0U << std::optional<QString>("1000") << QStringList{"set-own-password", "ada"}
            << Refusal::Usage;
        QTest::newRow("the password as an argument")
            << 0U << std::optional<QString>("1000") << QStringList{"set-own-password", "secret"}
            << Refusal::Usage;
    }

    void refusesBeforeReadingAnything() {
        QFETCH(const unsigned int, euid);
        QFETCH(const std::optional<QString>, pkexecUid);
        QFETCH(const QStringList, arguments);
        QFETCH(const Refusal, refusal);
        Run run(QByteArrayLiteral("new\n"));
        QCOMPARE(run.run(euid, pkexecUid, arguments).refusal, std::optional(refusal));
        QVERIFY(!run.read());
        QVERIFY(run.calls.isEmpty());
    }

    void refusesAnInputItCannotUse_data() {
        QTest::addColumn<std::optional<QByteArray>>("input");

        QTest::newRow("cannot be read") << std::optional<QByteArray>();
        QTest::newRow("nothing") << std::optional<QByteArray>("");
        QTest::newRow("only a line break") << std::optional<QByteArray>("\n");
        QTest::newRow("two lines") << std::optional<QByteArray>("one\ntwo\n");
        QTest::newRow("two line breaks") << std::optional<QByteArray>("one\n\n");
        QTest::newRow("a carriage return") << std::optional<QByteArray>("one\r\n");
        QTest::newRow("not UTF-8") << std::optional<QByteArray>("caf\xe9\n");
        QTest::newRow("a nul") << std::optional<QByteArray>(QByteArray("a\0b\n", 4));
        QTest::newRow("one character too long")
            << std::optional<QByteArray>(QByteArray(NewPassword::maxLength + 1, 'a') + '\n');
        QTest::newRow("far too long") << std::optional<QByteArray>(QByteArray(5000, 'a'));
    }

    void refusesAnInputItCannotUse() {
        QFETCH(const std::optional<QByteArray>, input);
        Run run(input);
        const OwnPasswordCommand::Outcome outcome = run.run(0, QStringLiteral("1000"));
        QCOMPARE(outcome.refusal, std::optional(Refusal::BadPassword));
        QCOMPARE(outcome.asked, QStringLiteral("set-own-password guardian"));
        QVERIFY(run.calls.isEmpty());
    }

    // The longest password the rule takes, in four-byte characters, fits
    // in what the command reads; a longer input is seen to be longer.
    void readsEnoughForTheLongestPassword() {
        const QString longest = QString(NewPassword::maxLength, QLatin1Char('x'))
                                    .replace(QLatin1Char('x'), QStringLiteral("🙂"));
        Run run(longest.toUtf8() + '\n');
        QCOMPARE(run.run(0, QStringLiteral("1000")).refusal, std::nullopt);
        QVERIFY(run.asked() >= longest.toUtf8().size() + 1);
    }

    // The real tool runner: the input goes on standard input, the
    // arguments stay empty, and a failing tool is a failure.
    void theToolGetsItOnStandardInput() {
        const QTemporaryDir folder;
        const QString chpasswd = QDir(folder.path()).filePath(QStringLiteral("chpasswd"));
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/read-input"), chpasswd));
        QVERIFY(SystemTool::runWithInput(chpasswd, {}, QByteArrayLiteral("guardian:a b\n")));
        QFile input(QDir(folder.path()).filePath(QStringLiteral("input")));
        QVERIFY(input.open(QIODevice::ReadOnly));
        QCOMPARE(input.readAll(), QByteArrayLiteral("guardian:a b\n"));
        QFile arguments(QDir(folder.path()).filePath(QStringLiteral("arguments")));
        QVERIFY(arguments.open(QIODevice::ReadOnly));
        QCOMPARE(arguments.readAll(), QByteArrayLiteral("\n"));

        QFile fail(QDir(folder.path()).filePath(QStringLiteral("fail")));
        QVERIFY(fail.open(QIODevice::WriteOnly));
        fail.close();
        QVERIFY(!SystemTool::runWithInput(chpasswd, {}, QByteArrayLiteral("guardian:a b\n")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(OwnPasswordCommandTest)
#include "tst_ownpasswordcommand.moc"
