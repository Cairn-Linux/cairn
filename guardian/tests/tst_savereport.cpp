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
        report.take(QStringLiteral("pictures made"));
        QCOMPARE(report.stage(), SaveReport::Stage::Looking);
        QCOMPARE(report.picturesKept(), std::optional<bool>(false));
        report.take(QStringLiteral("catalogue kept"));
        QCOMPARE(report.stage(), SaveReport::Stage::Writing);
        QCOMPARE(report.catalogueKept(), std::optional<bool>(true));
        report.take(QStringLiteral("waiting ada"));
        QCOMPARE(report.stage(), SaveReport::Stage::Waiting);
        QCOMPARE(report.current(), QStringLiteral("ada"));
        report.take(QStringLiteral("copying ada"));
        QCOMPARE(report.stage(), SaveReport::Stage::Copying);
        QCOMPARE(report.current(), QStringLiteral("ada"));
        report.take(QStringLiteral("child ada added 1 removed 0"));
        QCOMPARE(report.stage(), SaveReport::Stage::Writing);
        QCOMPARE(report.current(), QString());
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
        QTest::newRow("done") << "done";
        QTest::newRow("refused") << "refused busy";
        QTest::newRow("empty") << "";
    }

    void anyOtherLineIsLeftOut() {
        QFETCH(const QString, line);
        const SaveReport report = reportOf({line});
        QCOMPARE(report.stage(), SaveReport::Stage::Starting);
        QVERIFY(report.children().isEmpty());
        QVERIFY(!report.picturesKept().has_value());
    }
};

} // namespace

QTEST_GUILESS_MAIN(SaveReportTest)
#include "tst_savereport.moc"
