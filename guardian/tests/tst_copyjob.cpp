// SPDX-License-Identifier: Apache-2.0
#include "CopyJob.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

class CopyJobTest : public QObject {
    Q_OBJECT

    CopyJob::Result run(const QStringList& appIds,
                        CopyJob::IfLoggedIn ifLoggedIn = CopyJob::IfLoggedIn::Hold) {
        const CopyJob::Setup setup{.systemdRun = path("systemd-run"),
                                   .visitHelper = path("cairn-steam-visit")};
        return CopyJob::run(setup, "ada", 1002, appIds, ifLoggedIn,
                            [this](const QString& line) { m_said.append(line); });
    }

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    void give(const QString& name, const QByteArray& text) const {
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(text);
    }

    QByteArray read(const QString& name) const {
        QFile file(path(name));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    QStringList lines(const QString& name) const {
        return QString::fromUtf8(read(name)).split('\n', Qt::SkipEmptyParts);
    }

    QTemporaryDir m_temporary;
    QStringList m_said;

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        QDir folder(m_temporary.path());
        for (const QString& name : folder.entryList(QDir::Files | QDir::System)) {
            QVERIFY(folder.remove(name));
        }
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/systemd-run"), path("systemd-run")));
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/copy-helper"), path("cairn-steam-visit")));
        m_said.clear();
    }

    // The helper runs as a service of its own, by name, with nothing of
    // this program's environment and never --all.
    void runsTheHelperAsASystemJob() {
        const CopyJob::Result result = run({"620", "12"});
        QCOMPARE(lines("systemd-run-calls"),
                 QStringList({"--quiet --collect --wait --pipe --no-ask-password "
                              "--service-type=exec --expand-environment=no "
                              "--unit=cairn-copy-games-1002 "
                              "--description=Cairn copies games to a child -- " +
                              path("cairn-steam-visit") + " --copy-games ada 620 12"}));
        QCOMPARE(lines("helper-calls"), QStringList({"--copy-games ada 620 12"}));
        QCOMPARE(m_said, QStringList({"copied ada 620", "copied ada 12"}));
        QCOMPARE(result.copied, QStringList({"620", "12"}));
        QVERIFY(!result.held.has_value());
        const QString environment = QString::fromUtf8(read("systemd-run-environment"));
        QVERIFY(environment.contains("PATH=/usr/sbin:/usr/bin\n"));
        QVERIFY(environment.contains("LC_ALL=C\n"));
        for (const char* name : {"PKEXEC_UID=", "HOME=", "USER="}) {
            QVERIFY2(!environment.contains(QLatin1String("\n") + QLatin1String(name)) &&
                         !environment.startsWith(QLatin1String(name)),
                     name);
        }
        const QString call = lines("systemd-run-calls").constFirst();
        for (const char* never : {"--all", "--setenv", " -E ", "--uid", "--property"}) {
            QVERIFY2(!call.contains(QLatin1String(never)), never);
        }
    }

    // From a unit of its own, finish-games runs the helper itself, with
    // the same arguments and nothing of this program's environment.
    void runsTheHelperInThisUnit() {
        const CopyJob::Setup setup{.systemdRun = path("systemd-run"),
                                   .visitHelper = path("cairn-steam-visit"),
                                   .runs = CopyJob::Runs::InThisUnit};
        const CopyJob::Result result =
            CopyJob::run(setup, "ada", 1002, {"620", "12"}, CopyJob::IfLoggedIn::Hold,
                         [this](const QString& line) { m_said.append(line); });
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
        QCOMPARE(lines("helper-calls"), QStringList({"--copy-games ada 620 12"}));
        QCOMPARE(m_said, QStringList({"copied ada 620", "copied ada 12"}));
        QCOMPARE(result.copied, QStringList({"620", "12"}));
        QVERIFY(!result.held.has_value());
        // No systemd-run cleans the environment on this path, so this does.
        const QString environment = QString::fromUtf8(read("helper-environment"));
        QVERIFY(environment.contains("PATH=/usr/sbin:/usr/bin\n"));
        QVERIFY(environment.contains("LC_ALL=C\n"));
        for (const char* name : {"PKEXEC_UID=", "HOME=", "USER="}) {
            QVERIFY2(!environment.contains(QLatin1String("\n") + QLatin1String(name)) &&
                         !environment.startsWith(QLatin1String(name)),
                     name);
        }
    }

    // A helper that is not there says nothing: held as `not-started`.
    void aMissingHelperInThisUnitIsNotStarted() {
        QVERIFY(QFile::remove(path("cairn-steam-visit")));
        const CopyJob::Setup setup{.systemdRun = path("systemd-run"),
                                   .visitHelper = path("cairn-steam-visit"),
                                   .runs = CopyJob::Runs::InThisUnit};
        const CopyJob::Result result = CopyJob::run(
            setup, "ada", 1002, {"620"}, CopyJob::IfLoggedIn::Hold, [](const QString&) {});
        QCOMPARE(result.held, std::optional<QString>("not-started"));
    }

    // A child the parent chose to log out: --log-out goes straight after
    // --copy-games, and nowhere else.
    void logsOutWhenAsked() {
        give("lines-ada", "logging-out\nlogged-out\ncopied 620\nfinished\n");
        const CopyJob::Result result = run({"620"}, CopyJob::IfLoggedIn::LogOut);
        QCOMPARE(lines("systemd-run-calls"),
                 QStringList({"--quiet --collect --wait --pipe --no-ask-password "
                              "--service-type=exec --expand-environment=no "
                              "--unit=cairn-copy-games-1002 "
                              "--description=Cairn copies games to a child -- " +
                              path("cairn-steam-visit") + " --copy-games --log-out ada 620"}));
        QCOMPARE(lines("helper-calls"), QStringList({"--copy-games --log-out ada 620"}));
        QCOMPARE(m_said, QStringList({"logging-out ada", "logged-out ada", "copied ada 620"}));
        QCOMPARE(result.copied, QStringList({"620"}));
        QVERIFY(!result.held.has_value());
    }

    // A program of the child's outlived the log-out: held as `stuck`.
    void stuckLogOut() {
        give("lines-ada", "logging-out\nrefused stuck\n");
        give("status-ada", "1");
        const CopyJob::Result result = run({"620"}, CopyJob::IfLoggedIn::LogOut);
        QCOMPARE(m_said, QStringList({"logging-out ada"}));
        QVERIFY(result.copied.isEmpty());
        QCOMPARE(result.held, std::optional<QString>("stuck"));
    }

    // Nothing to copy starts nothing, and logs nobody out.
    void nothingToCopy_data() {
        QTest::addColumn<bool>("logOut");
        QTest::newRow("hold") << false;
        QTest::newRow("log out") << true;
    }

    void nothingToCopy() {
        QFETCH(const bool, logOut);
        const CopyJob::Result result =
            run({}, logOut ? CopyJob::IfLoggedIn::LogOut : CopyJob::IfLoggedIn::Hold);
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
        QVERIFY(result.copied.isEmpty());
        QVERIFY(!result.held.has_value());
    }

    // Only the helper's own lines about the games asked for go on, each
    // with the login from the request.
    void passesOnlyTheHelpersLines() {
        give("lines-ada", "waiting\n"
                          "copied 620\n"
                          "have 12\n"
                          "skipped 7 not-installed\n"
                          "copied 999\n"
                          "copied ben 620\n"
                          "copied 620 /home/ada\n"
                          "skipped 12 Not Installed\n"
                          "unnumbered\n"
                          "\x1b]0;title\x07\n"
                          "have 0620\n" +
                              QByteArray(300, 'x') + "finished\n" + "finished\n");
        const CopyJob::Result result = run({"620", "12", "7"});
        QCOMPARE(m_said, QStringList({"waiting ada", "copied ada 620", "have ada 12",
                                      "skipped ada 7 not-installed"}));
        QCOMPARE(result.copied, QStringList({"620", "12"}));
        QVERIFY(!result.held.has_value());
    }

    // Only so many lines are read; the copy still ends as it says.
    void readsAtMostSoManyLines() {
        QByteArray text;
        for (int i = 0; i < CopyJob::mostLines + 10; ++i) {
            text += "have 620\n";
        }
        give("lines-ada", text + "copied 12\nfinished\n");
        const CopyJob::Result result = run({"620", "12"});
        QCOMPARE(m_said.size(), CopyJob::mostLines);
        QCOMPARE(result.copied, QStringList({"620"}));
        // The end was not read, so the copy is not known to have finished.
        QCOMPARE(result.held, std::optional<QString>("failed"));
    }

    // A copy is waited for, however long it takes.
    void waitsForTheCopy() {
        give("sleep-ada", "3");
        QElapsedTimer timer;
        timer.start();
        const CopyJob::Result result = run({"620"});
        QVERIFY(timer.elapsed() >= 3000);
        QCOMPARE(result.copied, QStringList({"620"}));
        QVERIFY(!result.held.has_value());
    }

    // systemd-run fails with nothing from the helper: the name is taken.
    void notStarted() {
        give("fail-start", "");
        QCOMPARE(run({"620"}).held, std::optional<QString>("not-started"));
        QVERIFY(!QFile::exists(path("helper-calls")));
        QVERIFY(m_said.isEmpty());
    }

    void holds_data() {
        QTest::addColumn<QByteArray>("lines");
        QTest::addColumn<QString>("held");
        QTest::newRow("logged in") << QByteArray("refused logged-in\n") << "logged-in";
        QTest::newRow("a visit") << QByteArray("refused busy\n") << "busy";
        QTest::newRow("another copy") << QByteArray("refused copying\n") << "copying";
        QTest::newRow("still running")
            << QByteArray("waiting\nrefused still-running\n") << "still-running";
        QTest::newRow("stopped") << QByteArray("refused stopped\n") << "stopped";
        QTest::newRow("not set up") << QByteArray("refused not-set-up\n") << "not-set-up";
        QTest::newRow("stuck") << QByteArray("logging-out\nrefused stuck\n") << "stuck";
        QTest::newRow("a refusal that is a mistake")
            << QByteArray("refused not-for-pkexec\n") << "failed";
        QTest::newRow("failed") << QByteArray("copied 620\nfailed\n") << "failed";
        QTest::newRow("no library") << QByteArray("no-library\nfinished\n") << "no-library";
        QTest::newRow("no shared") << QByteArray("no-shared\nfinished\n") << "no-shared";
        QTest::newRow("lists shared") << QByteArray("lists-shared\nfinished\n") << "lists-shared";
        QTest::newRow("unreadable")
            << QByteArray("unreadable-config\nfinished\n") << "unreadable-config";
        QTest::newRow("no reflink") << QByteArray("no-reflink\nfailed\n") << "no-reflink";
        QTest::newRow("no last word") << QByteArray("copied 620\n") << "failed";
        QTest::newRow("only garbage") << QByteArray("hello\n") << "failed";
    }

    // Why the child's games were held back, in one word.
    void holds() {
        QFETCH(const QByteArray, lines);
        QFETCH(const QString, held);
        give("lines-ada", lines);
        give("status-ada", "1");
        QCOMPARE(run({"620"}).held, std::optional<QString>(held));
    }
};

} // namespace

QTEST_GUILESS_MAIN(CopyJobTest)
#include "tst_copyjob.moc"
