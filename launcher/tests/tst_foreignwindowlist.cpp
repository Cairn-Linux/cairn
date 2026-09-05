// SPDX-License-Identifier: Apache-2.0
#include "ForeignWindowList.h"

#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>

namespace {

// Tests run offscreen, where there is no compositor. The list must be safe to
// create there and say so by staying unavailable. The Wayland path itself is
// checked by hand under nested labwc; see launcher/README.md.
class ForeignWindowListTest : public QObject {
    Q_OBJECT

private slots:
    void unavailableWithoutACompositor() {
        const ForeignWindowList list(this);
        QVERIFY(!list.available());
        const QSignalSpy opened(&list, &ForeignWindowList::windowOpened);
        QTest::qWait(50);
        QCOMPARE(opened.count(), 0);
    }

    void ownAppIdIsTheExecutableNameByDefault() {
        QGuiApplication::setDesktopFileName(QString());
        const ForeignWindowList list(this);
        QCOMPARE(list.ownAppId(), QStringLiteral("tst_foreignwindowlist"));
    }

    void ownAppIdIsTheDesktopFileNameWhenSet() {
        QGuiApplication::setDesktopFileName(QStringLiteral("cairn-launcher"));
        const ForeignWindowList list(this);
        QCOMPARE(list.ownAppId(), QStringLiteral("cairn-launcher"));
    }
};

} // namespace

QTEST_MAIN(ForeignWindowListTest)
#include "tst_foreignwindowlist.moc"
