// SPDX-License-Identifier: Apache-2.0
#include "AppTree.h"
#include "Fixture.h"

#include <QTest>

namespace {

class AppTreeTest : public QObject {
    Q_OBJECT

private slots:
    void slugsAreLowercaseAsciiWithDashes_data() {
        QTest::addColumn<QString>("title");
        QTest::addColumn<QString>("expected");
        QTest::newRow("word") << "Draw" << "draw";
        QTest::newRow("two words") << "Tux Paint" << "tux-paint";
        QTest::newRow("punctuation") << "Putt-Putt: Moon!" << "putt-putt-moon";
        QTest::newRow("accents dropped") << "Café Ünïcode" << "cafe-unicode";
        QTest::newRow("spaces around") << "  Story  " << "story";
        QTest::newRow("digits") << "Freddi Fish 2" << "freddi-fish-2";
    }

    void slugsAreLowercaseAsciiWithDashes() {
        QFETCH(const QString, title);
        QFETCH(const QString, expected);
        QCOMPARE(AppTree::slug(title), expected);
    }

    void foldersFollowKindOrderAndSkipEmptyKinds() {
        const AppTree tree = fixtureTree();
        QCOMPARE(tree.folders(), (QStringList{QStringLiteral("make"), QStringLiteral("practice"),
                                              QStringLiteral("machine")}));
        const AppTree onlyMake(
            {{.title = QStringLiteral("Draw"), .kind = AppTree::Kind::Make, .exec = {}}});
        QCOMPARE(onlyMake.folders(), QStringList{QStringLiteral("make")});
        const AppTree empty({});
        QVERIFY(empty.folders().isEmpty());
    }

    void entriesKeepManifestOrderWithinAFolder() {
        const QList<AppTree::Entry> make = fixtureTree().entries(AppTree::Kind::Make);
        QCOMPARE(make.size(), 3);
        QCOMPARE(make[0].name, QStringLiteral("draw"));
        QCOMPARE(make[1].name, QStringLiteral("music"));
        QCOMPARE(make[2].name, QStringLiteral("tux-paint"));
        QCOMPARE(make[2].title, QStringLiteral("Tux Paint"));
        QCOMPARE(make[0].exec, QStringList{QStringLiteral("tuxpaint")});
    }

    void repeatedNamesAreNumbered() {
        const AppTree tree({
            {.title = QStringLiteral("Draw"), .kind = AppTree::Kind::Make, .exec = {}},
            {.title = QStringLiteral("draw"), .kind = AppTree::Kind::Make, .exec = {}},
            {.title = QStringLiteral("DRAW!"), .kind = AppTree::Kind::Practice, .exec = {}},
        });
        QCOMPARE(tree.entries(AppTree::Kind::Make)[1].name, QStringLiteral("draw-2"));
        QCOMPARE(tree.entries(AppTree::Kind::Practice)[0].name, QStringLiteral("draw-3"));
    }

    void aTitleWithNoLettersStillGetsAName() {
        const AppTree tree(
            {{.title = QStringLiteral("!!!"), .kind = AppTree::Kind::Make, .exec = {}}});
        QCOMPARE(tree.entries(AppTree::Kind::Make)[0].name, QStringLiteral("thing"));
    }

    void findLooksInOneFolderAndFindAnywhereInAll() {
        const AppTree tree = fixtureTree();
        QVERIFY(tree.find(AppTree::Kind::Make, QStringLiteral("draw")));
        QVERIFY(!tree.find(AppTree::Kind::Practice, QStringLiteral("draw")));
        QVERIFY(tree.findAnywhere(QStringLiteral("draw")));
        const AppTree::Entry practice =
            tree.findAnywhere(QStringLiteral("practice")).value_or(AppTree::Entry{});
        QCOMPARE(practice.kind, AppTree::Kind::Practice);
        QVERIFY(!tree.findAnywhere(QStringLiteral("nothing")));
    }

    void folderNamesRoundTrip() {
        QCOMPARE(AppTree::folderKind(QStringLiteral("make")), AppTree::Kind::Make);
        QCOMPARE(AppTree::folderKind(QStringLiteral("practice")), AppTree::Kind::Practice);
        QCOMPARE(AppTree::folderKind(QStringLiteral("machine")), AppTree::Kind::Machine);
        QVERIFY(!AppTree::folderKind(QStringLiteral("games")));
        QVERIFY(!AppTree::folderKind(QString()));
    }
};

} // namespace

QTEST_GUILESS_MAIN(AppTreeTest)
#include "tst_apptree.moc"
