// SPDX-License-Identifier: Apache-2.0
#include "ChildrenPage.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <cstdlib>

int main(int argc, char* argv[]) {
    const QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("cairn-guardian"));
    QGuiApplication::setOrganizationName(QStringLiteral("Cairn Linux"));
    // The menu entry, so the taskbar shows its name and icon.
    QGuiApplication::setDesktopFileName(QStringLiteral("cairn-guardian"));

    ChildrenPage::Setup setup;
    setup.command = QStringLiteral(CAIRN_FAMILY);
    ChildrenPage page(setup);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("page"), QVariant::fromValue(&page)}});
    engine.loadFromModule("Cairn.Family", "FamilyWindow");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
