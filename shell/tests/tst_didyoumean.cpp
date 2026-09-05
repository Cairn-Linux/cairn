// SPDX-License-Identifier: Apache-2.0
#include "DidYouMean.h"

#include <QTest>

namespace {

QStringList commands() {
    return {QStringLiteral("ls"), QStringLiteral("cd"), QStringLiteral("open"),
            QStringLiteral("cat"), QStringLiteral("help")};
}

class DidYouMeanTest : public QObject {
    Q_OBJECT

private slots:
    void editDistanceCountsInsertsDeletesAndSwaps_data() {
        QTest::addColumn<QString>("from");
        QTest::addColumn<QString>("to");
        QTest::addColumn<int>("expected");
        QTest::newRow("same") << "open" << "open" << 0;
        QTest::newRow("missing letter") << "opn" << "open" << 1;
        QTest::newRow("extra letter") << "opeen" << "open" << 1;
        QTest::newRow("wrong letter") << "opan" << "open" << 1;
        QTest::newRow("swapped letters") << "hlep" << "help" << 2;
        QTest::newRow("empty") << "" << "help" << 4;
        QTest::newRow("unrelated") << "rm" << "ls" << 2;
    }

    void editDistanceCountsInsertsDeletesAndSwaps() {
        QFETCH(const QString, from);
        QFETCH(const QString, to);
        QFETCH(const int, expected);
        QCOMPARE(DidYouMean::editDistance(from, to), expected);
    }

    void closestSuggests_data() {
        QTest::addColumn<QString>("word");
        QTest::addColumn<QString>("expected");
        QTest::newRow("opn") << "opn" << "open";
        QTest::newRow("hlep") << "hlep" << "help";
        QTest::newRow("lss") << "lss" << "ls";
        QTest::newRow("exact") << "cat" << "cat";
        // A short word may be one edit away; "rm" is two from ls and cd.
        QTest::newRow("rm") << "rm" << "";
        QTest::newRow("sudo") << "sudo" << "";
        QTest::newRow("dir") << "dir" << "";
        // A tie goes to the earlier candidate: cd comes before cat.
        QTest::newRow("tie") << "ct" << "cd";
    }

    void closestSuggests() {
        QFETCH(const QString, word);
        QFETCH(const QString, expected);
        const std::optional<QString> got = DidYouMean::closest(word, commands());
        QCOMPARE(got.value_or(QString()), expected);
    }

    void noCandidatesMeansNoSuggestion() {
        QVERIFY(!DidYouMean::closest(QStringLiteral("draw"), {}));
    }
};

} // namespace

QTEST_GUILESS_MAIN(DidYouMeanTest)
#include "tst_didyoumean.moc"
