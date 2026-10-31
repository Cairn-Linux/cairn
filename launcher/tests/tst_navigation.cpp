// SPDX-License-Identifier: Apache-2.0
#include <QCoreApplication>
#include <QKeyEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

namespace {

// TestCase.keyPress cannot mark a press as the keyboard's own repeat, which
// is what a child holding a key down sends. The tests get it as `heldKey`.
class HeldKey : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty("heldKey", this);
    }

    // One repeat of `key`, delivered to whatever has the focus in `window`.
    void repeat(QObject* window, int key) {
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, QString(), true);
        QCoreApplication::sendEvent(window, &event);
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(launcher, HeldKey)
#include "tst_navigation.moc"
