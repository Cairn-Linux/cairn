// SPDX-License-Identifier: Apache-2.0
#include "ToolsRefresh.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

class ToolsRefreshTest : public QObject {
    Q_OBJECT

    ToolsRefresh::Result run(const std::optional<QString>& guardian) const {
        ToolsRefresh::Setup setup;
        setup.systemdRun = path("systemd-run");
        setup.python = path("python");
        setup.shareStep = QStringLiteral("/x/cairn-share-tools");
        return ToolsRefresh::run(setup, guardian);
    }

    static ToolsRefresh::Result take(const QStringList& lines) {
        ToolsRefresh::Result result;
        for (const QString& line : lines) {
            ToolsRefresh::take(result, line);
        }
        return result;
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

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        QDir folder(m_temporary.path());
        for (const QString& name : folder.entryList(QDir::Files | QDir::System)) {
            QVERIFY(folder.remove(name));
        }
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/systemd-run"), path("systemd-run")));
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/python"), path("python")));
    }

    // Under sudo there is no Guardian, and nothing starts.
    void noGuardianStartsNothing() {
        const ToolsRefresh::Result result = run(std::nullopt);
        QVERIFY(!result.made);
        QVERIFY(!result.endedEarly);
        QVERIFY(result.notes.isEmpty());
        QVERIFY(!QFile::exists(path("systemd-run-calls")));
        QVERIFY(!QFile::exists(path("python-calls")));
    }

    // The step runs as the Guardian, never as root, in a service of its own
    // that systemd stops after five minutes, with nothing of this program's
    // environment.
    void runsAsTheGuardianAsASystemJob() {
        const ToolsRefresh::Result result = run(QStringLiteral("gina"));
        QVERIFY(result.made);
        QVERIFY(result.notes.isEmpty());
        QCOMPARE(lines("systemd-run-calls"),
                 QStringList({"--quiet --collect --wait --pipe --no-ask-password "
                              "--service-type=exec --expand-environment=no "
                              "--unit=cairn-share-tools "
                              "--description=Cairn takes Steam's tools from a grown-up's Steam "
                              "--uid=gina --property=RuntimeMaxSec=300 -- " +
                              path("python") + " -I -B /x/cairn-share-tools"}));
        QCOMPARE(lines("python-calls"), QStringList({"-I -B /x/cairn-share-tools"}));
        const QString environment = QString::fromUtf8(read("python-environment"));
        for (const char* name : {"PKEXEC_UID=", "HOME=", "USER="}) {
            QVERIFY2(!environment.contains(QLatin1String("\n") + QLatin1String(name)) &&
                         !environment.startsWith(QLatin1String(name)),
                     name);
        }
    }

    // A step that did not finish and did not say why still leaves a word in
    // the journal: no-answer when it said no line, stopped when it said some.
    void madeOnlyWhenTheStepFinished_data() {
        QTest::addColumn<QString>("given");
        QTest::addColumn<QByteArray>("output");
        QTest::addColumn<bool>("made");
        QTest::addColumn<QStringList>("notes");
        QTest::newRow("finished") << "" << QByteArray("finished\n") << true << QStringList{};
        // The unit's name is still taken, or systemd is not there.
        QTest::newRow("not started")
            << "fail-start" << QByteArray() << false << QStringList{"no-answer"};
        QTest::newRow("failed with nothing said")
            << "fail-share-tools" << QByteArray() << false << QStringList{"no-answer"};
        // Stopped at its time limit, mid-way.
        QTest::newRow("stopped mid-way") << "" << QByteArray("tool-shared 1070560\n") << false
                                         << QStringList{"tool-shared 1070560", "stopped"};
        // A line that is not the step's still shows that it started.
        QTest::newRow("stopped after another line")
            << "" << QByteArray("something else\n") << false << QStringList{"stopped"};
        // no-steam comes before `finished`, so it ends nothing.
        QTest::newRow("stopped after no-steam")
            << "" << QByteArray("no-steam\n") << false << QStringList{"no-steam", "stopped"};
        QTest::newRow("no Steam") << "" << QByteArray("no-steam\nfinished\n") << true
                                  << QStringList{"no-steam"};
        // A step that says why it ended gets no word of cairn-family's own.
        QTest::newRow("failed") << "" << QByteArray("failed\n") << false << QStringList{"failed"};
        QTest::newRow("failed mid-way") << "" << QByteArray("tool-shared 1070560\nfailed\n")
                                        << false << QStringList{"tool-shared 1070560", "failed"};
        QTest::newRow("refused folder")
            << "" << QByteArray("refused folder\n") << false << QStringList{"refused folder"};
        QTest::newRow("refused root")
            << "" << QByteArray("refused root\n") << false << QStringList{"refused root"};
    }

    void madeOnlyWhenTheStepFinished() {
        QFETCH(const QString, given);
        QFETCH(const QByteArray, output);
        QFETCH(const bool, made);
        QFETCH(const QStringList, notes);
        if (!given.isEmpty()) {
            give(given, "");
        }
        if (!output.isEmpty()) {
            give("share-tools-output", output);
        }
        const ToolsRefresh::Result result = run(QStringLiteral("gina"));
        QCOMPARE(result.made, made);
        QCOMPARE(result.notes, notes);
    }

    // The Save says the tools left out; the journal gets every line worth
    // one.
    void toolsLeftOutAndNotes() {
        give("share-tools-output", "tool-shared 1070560\n"
                                   "tool-had 1391110\n"
                                   "tool-kept 1493710 downloading\n"
                                   "tool-not-shared 4183110 other-drive\n"
                                   "finished\n");
        const ToolsRefresh::Result result = run(QStringLiteral("gina"));
        QVERIFY(result.made);
        QCOMPARE(result.leftOut, QStringList({"tool-not-shared 4183110 other-drive"}));
        QCOMPARE(result.notes, QStringList({"tool-shared 1070560", "tool-kept 1493710 downloading",
                                            "tool-not-shared 4183110 other-drive"}));
    }

    // A line far longer than any of the step's is dropped by SystemJob, and
    // no part of it is read as a line.
    void aLongLineIsNotALine() {
        give("share-tools-output", "tool-not-shared 1 " + QByteArray(300, 'x') + "\nfinished\n");
        const ToolsRefresh::Result result = run(QStringLiteral("gina"));
        QVERIFY(result.made);
        QVERIFY(result.notes.isEmpty());
    }

    void takesOnlyTheStepsOwnWords_data() {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QStringList>("notes");
        QTest::addColumn<QStringList>("leftOut");
        QTest::newRow("shared") << "tool-shared 1070560" << QStringList{"tool-shared 1070560"}
                                << QStringList{};
        QTest::newRow("had") << "tool-had 4183110" << QStringList{} << QStringList{};
        QTest::newRow("kept") << "tool-kept 1 not-installed"
                              << QStringList{"tool-kept 1 not-installed"} << QStringList{};
        QTest::newRow("not shared") << "tool-not-shared 1 unreadable-files"
                                    << QStringList{"tool-not-shared 1 unreadable-files"}
                                    << QStringList{"tool-not-shared 1 unreadable-files"};
        for (const char* word : {"no-steam", "failed", "refused root", "refused folder"}) {
            QTest::newRow(word) << word << QStringList{word} << QStringList{};
        }
        for (const char* line :
             {"tool-shared 01070560", "tool-shared 12345678901", "tool-shared 1 extra",
              "tool-shared", "tool-kept 1", "tool-kept 1 Downloading", "tool-kept 1 /var/home/gina",
              "tool-not-shared 1 not--installed", "tool-not-shared 1 -downloading",
              "tool-not-shared Proton 9.0 downloading", "tool-shared /var/home/gina", "refused",
              "refused something", " failed", "failed ", "tool-copied 1070560", "tools made",
              "copying-tools", ""}) {
            QTest::newRow(*line == '\0' ? "empty" : line) << line << QStringList{} << QStringList{};
        }
    }

    void takesOnlyTheStepsOwnWords() {
        QFETCH(const QString, line);
        QFETCH(const QStringList, notes);
        QFETCH(const QStringList, leftOut);
        const ToolsRefresh::Result result = take({line});
        QCOMPARE(result.notes, notes);
        QCOMPARE(result.leftOut, leftOut);
        QVERIFY(!result.made);
    }

    // Only the word itself is the end.
    void finishedIsTheOnlyEnd() {
        QVERIFY(take({"finished"}).made);
        QVERIFY(!take({"finished now"}).made);
        QVERIFY(!take({"Finished"}).made);
        QVERIFY(take({"finished"}).notes.isEmpty());
    }

    // Only the step's three words for an early end say why it ended.
    void onlyItsOwnWordsSayWhyItEndedEarly() {
        for (const char* word : {"failed", "refused root", "refused folder"}) {
            QVERIFY2(take({word}).endedEarly, word);
        }
        for (const char* line :
             {"no-steam", "finished", "tool-shared 1", "tool-not-shared 1 copy-failed",
              "failed now", "refused", "refused something"}) {
            QVERIFY2(!take({line}).endedEarly, line);
        }
    }
};

} // namespace

QTEST_GUILESS_MAIN(ToolsRefreshTest)
#include "tst_toolsrefresh.moc"
