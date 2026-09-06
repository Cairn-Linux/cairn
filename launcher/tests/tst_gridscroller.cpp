// SPDX-License-Identifier: Apache-2.0
#include "GridScroller.h"

#include <QSignalSpy>
#include <QTest>

namespace {

// Three columns, two rows showing, fourteen tiles: five rows.
GridScroller* manyTiles(QObject* parent) {
    auto* scroller = new GridScroller(parent);
    scroller->setColumns(3);
    scroller->setVisibleRows(2);
    scroller->setCount(14);
    return scroller;
}

class GridScrollerTest : public QObject {
    Q_OBJECT

private slots:
    void sixTilesNeverScroll() {
        GridScroller scroller(this);
        scroller.setCount(6);
        QCOMPARE(scroller.rows(), 2);
        QVERIFY(!scroller.scrolls());
        scroller.setCurrentIndex(5);
        QCOMPARE(scroller.firstRow(), 0);
        QCOMPARE(scroller.indexAfterWheel(-120), 5);
    }

    void countsRows() {
        GridScroller* scroller = manyTiles(this);
        QCOMPARE(scroller->rows(), 5);
        QVERIFY(scroller->scrolls());
        scroller->setCount(0);
        QCOMPARE(scroller->rows(), 0);
        QCOMPARE(scroller->firstRow(), 0);
    }

    void windowFollowsTheFocusDownOneRowAtATime() {
        GridScroller* scroller = manyTiles(this);
        const QSignalSpy moves(scroller, &GridScroller::firstRowChanged);
        scroller->setCurrentIndex(3); // row 1, still in the window
        QCOMPARE(scroller->firstRow(), 0);
        scroller->setCurrentIndex(6); // row 2: the window slides one row
        QCOMPARE(scroller->firstRow(), 1);
        scroller->setCurrentIndex(13); // row 4, the last
        QCOMPARE(scroller->firstRow(), 3);
        QCOMPARE(moves.count(), 2);
    }

    void windowFollowsTheFocusUpOnlyWhenItLeaves() {
        GridScroller* scroller = manyTiles(this);
        scroller->setCurrentIndex(13);
        QCOMPARE(scroller->firstRow(), 3);
        scroller->setCurrentIndex(9); // row 3, the window's top row: stays
        QCOMPARE(scroller->firstRow(), 3);
        scroller->setCurrentIndex(6); // row 2: slides up one
        QCOMPARE(scroller->firstRow(), 2);
        scroller->setCurrentIndex(0);
        QCOMPARE(scroller->firstRow(), 0);
    }

    void wrappingToTheTopBringsTheWindowBack() {
        GridScroller* scroller = manyTiles(this);
        scroller->setCurrentIndex(12);
        QCOMPARE(scroller->firstRow(), 3);
        scroller->setCurrentIndex(0);
        QCOMPARE(scroller->firstRow(), 0);
    }

    void wheelMovesOneRowAndStopsAtTheEnds() {
        GridScroller* scroller = manyTiles(this);
        QCOMPARE(scroller->indexAfterWheel(-120), 3);
        scroller->setCurrentIndex(11);                 // last full row's end
        QCOMPARE(scroller->indexAfterWheel(-120), 13); // into the short last row
        scroller->setCurrentIndex(13);
        QCOMPARE(scroller->indexAfterWheel(-120), 13);
        QCOMPARE(scroller->indexAfterWheel(120), 10);
        scroller->setCurrentIndex(1);
        QCOMPARE(scroller->indexAfterWheel(120), 1);
        QCOMPARE(scroller->indexAfterWheel(0), 1);
    }

    void fewerVisibleRowsSlideSooner() {
        GridScroller* scroller = manyTiles(this);
        scroller->setVisibleRows(1);
        scroller->setCurrentIndex(4);
        QCOMPARE(scroller->firstRow(), 1);
        scroller->setVisibleRows(5);
        QCOMPARE(scroller->firstRow(), 0);
        QVERIFY(!scroller->scrolls());
    }

    void shrinkingTheCountKeepsTheWindowInRange() {
        GridScroller* scroller = manyTiles(this);
        scroller->setCurrentIndex(13);
        QCOMPARE(scroller->firstRow(), 3);
        scroller->setCount(6);
        QCOMPARE(scroller->firstRow(), 0);
    }
};

} // namespace

QTEST_GUILESS_MAIN(GridScrollerTest)
#include "tst_gridscroller.moc"
