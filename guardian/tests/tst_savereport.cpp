// SPDX-License-Identifier: Apache-2.0
#include "SaveReport.h"

#include <QTest>

namespace {

SaveReport reportOf(const QStringList& lines) {
    SaveReport report({QStringLiteral("ada"), QStringLiteral("ben")});
    for (const QString& line : lines) {
        report.take(line);
    }
    return report;
}

class SaveReportTest : public QObject {
    Q_OBJECT

private slots:
    void itStartsWithNothingSaid() {
        const SaveReport report = reportOf({});
        QCOMPARE(report.stage(), SaveReport::Stage::Starting);
        QCOMPARE(report.current(), QString());
        QVERIFY(!report.picturesKept().has_value());
        QVERIFY(!report.catalogueKept().has_value());
        QVERIFY(report.children().isEmpty());
    }

    // Each line moves the stage on, as the command goes.
    void theStageFollowsTheLines() {
        SaveReport report({QStringLiteral("ada")});
        report.take(QStringLiteral("looking"));
        QCOMPARE(report.stage(), SaveReport::Stage::Looking);
        QVERIFY(!report.picturesKept().has_value());
        report.take(QStringLiteral("pictures made"));
        QCOMPARE(report.stage(), SaveReport::Stage::Looking);
        QCOMPARE(report.picturesKept(), std::optional<bool>(false));
        report.take(QStringLiteral("catalogue kept"));
        QCOMPARE(report.stage(), SaveReport::Stage::SharingTools);
        QCOMPARE(report.catalogueKept(), std::optional<bool>(true));
        report.take(QStringLiteral("tools made"));
        QCOMPARE(report.stage(), SaveReport::Stage::Writing);
        QVERIFY(!report.toolsLeftOut());
        report.take(QStringLiteral("waiting ada"));
        QCOMPARE(report.stage(), SaveReport::Stage::Waiting);
        QCOMPARE(report.current(), QStringLiteral("ada"));
        report.take(QStringLiteral("copying ada"));
        QCOMPARE(report.stage(), SaveReport::Stage::Copying);
        QCOMPARE(report.current(), QStringLiteral("ada"));
        report.take(QStringLiteral("copying-tools ada"));
        QCOMPARE(report.stage(), SaveReport::Stage::CopyingTools);
        QCOMPARE(report.current(), QStringLiteral("ada"));
        report.take(QStringLiteral("child ada added 1 removed 0"));
        QCOMPARE(report.stage(), SaveReport::Stage::Writing);
        QCOMPARE(report.current(), QString());
    }

    // What each child's copy did with Steam's tools (ADR-0046).
    void eachChildsTools() {
        const SaveReport report =
            reportOf({"copying ada", "copied ada 620", "copying-tools ada",
                      "tool-copied ada 1070560", "tool-copied ada 1391110", "tool-have ada 1493710",
                      "child ada added 1 removed 0", "copying ben", "have ben 620",
                      "tool-skipped ben 1070560 no-reflink", "child ben added 1 removed 0"});
        const QList<SaveReport::Child> children = report.children();
        QCOMPARE(children.size(), 2);
        QCOMPARE(children.at(0).toolsCopied, 2);
        QVERIFY(!children.at(0).toolsMissing);
        QVERIFY(!children.at(0).noTools);
        QCOMPARE(children.at(1).toolsCopied, 0);
        QVERIFY(children.at(1).toolsMissing);
        // A tool is never a game the child got.
        QCOMPARE(report.copied().size(), 2);
    }

    void aToolThatMayStillBeMissing_data() {
        QTest::addColumn<QString>("line");
        QTest::newRow("missing") << "tool-missing ada 1391110";
        QTest::newRow("skipped") << "tool-skipped ada 1070560 changed";
        QTest::newRow("unreadable folder") << "tools-unreadable ada";
    }

    void aToolThatMayStillBeMissing() {
        QFETCH(const QString, line);
        const SaveReport report = reportOf({"copying ada", line, "child ada added 1 removed 0"});
        QVERIFY(report.children().constFirst().toolsMissing);
        QVERIFY(!report.children().constFirst().noTools);
    }

    void noToolsToCopy() {
        const SaveReport report = reportOf(
            {"copying ada", "copied ada 620", "no-tools ada", "child ada added 1 removed 0"});
        QVERIFY(report.children().constFirst().noTools);
        QVERIFY(!report.children().constFirst().toolsMissing);
    }

    // Whether some tool stayed out of the tools folder: the step did not
    // finish, or left one out. Its lines name no child.
    void toolsLeftOut() {
        QVERIFY(!reportOf({"tools made"}).toolsLeftOut());
        QVERIFY(reportOf({"tools kept"}).toolsLeftOut());
        QVERIFY(reportOf({"tools made", "tool-not-shared 4183110 other-drive"}).toolsLeftOut());
        QVERIFY(reportOf({"tool-not-shared 4183110 other-drive"}).children().isEmpty());
        QVERIFY(!reportOf({"tool-not-shared 4183110 Other Drive"}).toolsLeftOut());
        QVERIFY(!reportOf({"tool-not-shared 04183110 other-drive"}).toolsLeftOut());
    }

    // Only a copy that got as far as a game or a tool gave a child
    // anything: one held before it began did not.
    void copiedForSomeone() {
        QVERIFY(
            !reportOf({"tools kept", "copying ada", "child ada added 0 removed 0 held logged-in"})
                 .copiedForSomeone());
        QVERIFY(!reportOf({"copying ada", "skipped ada 620 not-installed"}).copiedForSomeone());
        for (const char* line :
             {"copied ada 620", "have ada 620", "tool-have ada 1070560", "no-tools ada",
              "copying-tools ada", "tools-unreadable ada", "tool-skipped ada 1070560 no-reflink",
              "tool-missing ada 1391110"}) {
            QVERIFY2(reportOf({"copying ada", line}).copiedForSomeone(), line);
        }
    }

    // A tool line for a child the Save did not name is left out.
    void toolLinesForAnotherChild() {
        const SaveReport report =
            reportOf({"copying-tools cy", "tool-copied cy 1070560", "tool-missing cy 1391110",
                      "tool-skipped cy 1 changed", "no-tools cy", "tools-unreadable cy"});
        QCOMPARE(report.stage(), SaveReport::Stage::Starting);
        QVERIFY(report.children().isEmpty());
        QVERIFY(!report.copiedForSomeone());
    }

    void eachChildsCounts() {
        const SaveReport report =
            reportOf({"child ben added 0 removed 2", "child ada added 3 removed 1"});
        const QList<SaveReport::Child> children = report.children();
        QCOMPARE(children.size(), 2);
        QCOMPARE(children.at(0).login, QStringLiteral("ben"));
        QCOMPARE(children.at(0).added, std::optional<int>(0));
        QCOMPARE(children.at(0).removed, std::optional<int>(2));
        QVERIFY(!children.at(0).held.has_value());
        QCOMPARE(children.at(1).login, QStringLiteral("ada"));
        QCOMPARE(children.at(1).added, std::optional<int>(3));
    }

    void aHeldWord() {
        const SaveReport report = reportOf({"child ada added 0 removed 1 held logged-in"});
        QCOMPARE(report.children().constFirst().held, std::optional<QString>("logged-in"));
        QCOMPARE(report.children().constFirst().removed, std::optional<int>(1));
    }

    // A skipped game before the child's line is theirs, with its reason.
    void skippedGames() {
        const SaveReport report =
            reportOf({"skipped ada 1001 not-shared", "skipped ada 1002 folder-without-manifest",
                      "child ada added 0 removed 0"});
        const SaveReport::Child ada = report.children().constFirst();
        QCOMPARE(ada.skipped.size(), 2);
        QCOMPARE(ada.skipped.at(0).appId, QStringLiteral("1001"));
        QCOMPARE(ada.skipped.at(0).reason, QStringLiteral("not-shared"));
        QCOMPARE(ada.skipped.at(1).reason, QStringLiteral("folder-without-manifest"));
        QCOMPARE(ada.added, std::optional<int>(0));
    }

    // Each game copied, or found already there, in order, for the progress
    // window; neither is a child's result.
    void copiedGames() {
        const SaveReport report =
            reportOf({"copying ada", "copied ada 620", "have ada 12", "copied ben 504230",
                      "copied cy 620", "copied ada 0620", "copied ada 620 /home/ada"});
        const QList<SaveReport::Copied> copied = report.copied();
        QCOMPARE(copied.size(), 3);
        QCOMPARE(copied.at(0).login, QStringLiteral("ada"));
        QCOMPARE(copied.at(0).appId, QStringLiteral("620"));
        QVERIFY(!copied.at(0).had);
        QCOMPARE(copied.at(1).appId, QStringLiteral("12"));
        QVERIFY(copied.at(1).had);
        QCOMPARE(copied.at(2).login, QStringLiteral("ben"));
        QVERIFY(report.children().isEmpty());
        QCOMPARE(report.stage(), SaveReport::Stage::Copying);
    }

    // A Save cut off after a skip still names the child, with no counts.
    void aChildWithoutTheirLine() {
        const SaveReport report = reportOf({"skipped ben 1001 downloading"});
        QCOMPARE(report.children().size(), 1);
        QVERIFY(!report.children().constFirst().added.has_value());
    }

    // Only the command's words, and only the children the Save named.
    void anyOtherLineIsLeftOut_data() {
        QTest::addColumn<QString>("line");
        QTest::newRow("another child") << "child cy added 1 removed 0";
        QTest::newRow("another child copied") << "copying cy";
        QTest::newRow("another child skipped") << "skipped cy 1001 not-shared";
        QTest::newRow("another child's game") << "copied cy 620";
        QTest::newRow("a game with no child") << "copied 620";
        QTest::newRow("a count too long") << "child ada added 12345 removed 0";
        QTest::newRow("a negative count") << "child ada added -1 removed 0";
        QTest::newRow("a held word in capitals") << "child ada added 0 removed 0 held LOGGED-IN";
        QTest::newRow("held and no word") << "child ada added 0 removed 0 held";
        QTest::newRow("words after the line") << "child ada added 1 removed 0 and more";
        QTest::newRow("an appid with a leading 0") << "skipped ada 0123 not-shared";
        QTest::newRow("a path") << "skipped ada 1001 /home/ada";
        QTest::newRow("a login with a capital") << "copying Ada";
        QTest::newRow("spaces before") << " child ada added 1 removed 0";
        QTest::newRow("pictures, another word") << "pictures lost";
        QTest::newRow("tools, another word") << "tools lost";
        QTest::newRow("a tool with no child") << "tool-copied 1070560";
        QTest::newRow("a tool's appid with a leading 0") << "tool-copied ada 01070560";
        QTest::newRow("a tool's word in capitals") << "tool-skipped ada 1 Changed";
        QTest::newRow("a tool skipped with no word") << "tool-skipped ada 1";
        QTest::newRow("copying tools with no child") << "copying-tools";
        QTest::newRow("words after a tool") << "tool-copied ada 1 and more";
        QTest::newRow("done") << "done";
        QTest::newRow("refused") << "refused busy";
        QTest::newRow("empty") << "";
    }

    void anyOtherLineIsLeftOut() {
        QFETCH(const QString, line);
        const SaveReport report = reportOf({line});
        QCOMPARE(report.stage(), SaveReport::Stage::Starting);
        QVERIFY(report.children().isEmpty());
        QVERIFY(report.copied().isEmpty());
        QVERIFY(!report.picturesKept().has_value());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SaveReportTest)
#include "tst_savereport.moc"
