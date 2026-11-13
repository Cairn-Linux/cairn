// SPDX-License-Identifier: Apache-2.0
#include "AboutPage.h"
#include "AccountPage.h"
#include "ChildrenPage.h"
#include "GamesPage.h"
#include "NoticesPage.h"

#include <QDBusConnection>
#include <QDesktopServices>
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
    accountSetup.walletBus = QDBusConnection::sessionBus();
    AccountPage accountPage(accountSetup);

    NoticesPage::Setup noticesSetup;
    noticesSetup.uid = getuid();
    NoticesPage noticesPage(noticesSetup);

    // The desktop opens an address in the grown-up's own web browser or
    // file manager, and only when a button on the About page is pressed.
    // Family itself makes no network call. Under Wayland Qt answers yes at
    // once, whatever happens next, so there the page never learns that
    // nothing opened.
    AboutPage::Setup aboutSetup;
    aboutSetup.open = [](const QUrl& address) { return QDesktopServices::openUrl(address); };
    AboutPage aboutPage(aboutSetup);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("page"), QVariant::fromValue(&page)},
        {QStringLiteral("gamesPage"), QVariant::fromValue(&gamesPage)},
        {QStringLiteral("accountPage"), QVariant::fromValue(&accountPage)},
        {QStringLiteral("noticesPage"), QVariant::fromValue(&noticesPage)},
        {QStringLiteral("aboutPage"), QVariant::fromValue(&aboutPage)},
    });
    engine.loadFromModule("Cairn.Family", "FamilyWindow");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
