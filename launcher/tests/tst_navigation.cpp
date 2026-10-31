// SPDX-License-Identifier: Apache-2.0
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QtQuickTest/quicktest.h>

namespace {

// Gives the tests the folder CMake put a stand-in game picture in, as the
// picture step's folder holds one for each game (ADR-0047).
class Setup : public QObject {
    Q_OBJECT

public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("standInPictures"),
                                                  QStringLiteral(STAND_IN_PICTURES));
    }
};

} // namespace

QUICK_TEST_MAIN_WITH_SETUP(launcher, Setup)
#include "tst_navigation.moc"
