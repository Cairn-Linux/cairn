// SPDX-License-Identifier: Apache-2.0
#include "ChildrenPage.h"
#include "GamesPage.h"

#include <QDBusConnection>
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

    GamesPage::Setup gamesSetup;
    gamesSetup.command = QStringLiteral(CAIRN_FAMILY);
    gamesSetup.steamFor = QStringLiteral(CAIRN_STEAM_FOR);
    gamesSetup.loginBus = QDBusConnection::systemBus();
    GamesPage gamesPage(gamesSetup);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("page"), QVariant::fromValue(&page)},
        {QStringLiteral("gamesPage"), QVariant::fromValue(&gamesPage)},
    });
    engine.loadFromModule("Cairn.Family", "FamilyWindow");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
