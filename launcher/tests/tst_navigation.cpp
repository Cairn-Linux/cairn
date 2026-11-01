// SPDX-License-Identifier: Apache-2.0
#include <QCoreApplication>
#include <QKeyEvent>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

namespace {

// Gives the tests the folder CMake put a stand-in game picture in, as the
// picture step's folder holds one for each game (ADR-0047). And, as
// `heldKey`, the keyboard's own repeat of a held key, which a child holding
// a key down sends and TestCase.keyPress cannot mark.
class Setup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("standInPictures"),
                                                  QStringLiteral(STAND_IN_PICTURES));
        engine->rootContext()->setContextProperty(QStringLiteral("heldKey"), this);
    }

    // One repeat of `key`, delivered to whatever has the focus in `window`.
    void repeat(QObject* window, int key) {
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier, QString(), true);
        QCoreApplication::sendEvent(window, &event);
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(launcher, Setup)
#include "tst_navigation.moc"
