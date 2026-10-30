// SPDX-License-Identifier: Apache-2.0
#include "AccountPage.h"
#include "ChildrenPage.h"
#include "GamesPage.h"

#include <QDBusConnection>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <cstdlib>
#include <unistd.h>

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
    gamesSetup.steamHome = QDir::homePath();
    GamesPage gamesPage(gamesSetup);

    AccountPage::Setup accountSetup;
    accountSetup.command = QStringLiteral(CAIRN_FAMILY);
    accountSetup.uid = getuid();
    accountSetup.bus = QDBusConnection::systemBus();
    AccountPage accountPage(accountSetup);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("page"), QVariant::fromValue(&page)},
        {QStringLiteral("gamesPage"), QVariant::fromValue(&gamesPage)},
        {QStringLiteral("accountPage"), QVariant::fromValue(&accountPage)},
    });
    engine.loadFromModule("Cairn.Family", "FamilyWindow");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
