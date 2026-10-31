// SPDX-License-Identifier: Apache-2.0
#include "GiveUpWatch.h"
#include "SteamAtLogin.h"
#include "TileFiles.h"
#include "UPowerDevice.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <cstdlib>

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("cairn-launcher"));
    QGuiApplication::setOrganizationName(QStringLiteral("Cairn Linux"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption manifestOption(
        QStringLiteral("manifest"),
        QStringLiteral("A kidscan manifest (version 1) that names the tiles and what they run."),
        QStringLiteral("file"));
    parser.addOption(manifestOption);
    const QCommandLineOption gamesOption(
        QStringLiteral("games"),
        QStringLiteral("The games file (version 1) that provisioning makes from the shared Steam "
                       "library. Its tiles follow the manifest's, and each may only start a Steam "
                       "game."),
        QStringLiteral("file"));
    parser.addOption(gamesOption);
    // The image's and provisioning's wrappers name it; a launcher installed
    // by hand under an older wrapper still finds the pictures.
    const QCommandLineOption picturesOption(
        QStringLiteral("pictures"),
        QStringLiteral("The folder of the games' pictures, <appid>.jpg, which the picture step "
                       "copies from a grown-up's Steam. Each game from the games file shows its "
                       "own."),
        QStringLiteral("folder"), QStringLiteral("/var/lib/cairn/game-pictures"));
    parser.addOption(picturesOption);
    const QCommandLineOption logOutOption(
        QStringLiteral("log-out"),
        QStringLiteral("The program that ends the session when the child chooses Log out. "
                       "Without it there is no Log out."),
        QStringLiteral("program"));
    parser.addOption(logOutOption);
    const QCommandLineOption scopeAppsOption(
        QStringLiteral("scope-apps"),
        QStringLiteral("Start each program in a systemd user scope of its own, so the grown-up's "
                       "give-up key can end it. The kiosk session sets this."));
    parser.addOption(scopeAppsOption);
    const QCommandLineOption startSteamOption(
        QStringLiteral("start-steam"),
        QStringLiteral("Start the Steam client in the background when a tile runs a Steam game. "
                       "The kiosk session sets this."));
    parser.addOption(startSteamOption);
    parser.process(application);

    // The same tiles the child will see, so a child whose only Steam tiles
    // come from the games file still gets Steam at login (ADR-0033).
    const bool startSteam =
        parser.isSet(startSteamOption) &&
        SteamAtLogin::wanted(
            TileFiles::read(parser.value(manifestOption), parser.value(gamesOption)).tiles);
    if (startSteam) {
        SteamAtLogin::start();
    }

    QQmlApplicationEngine engine(&application);
    engine.setInitialProperties({
        {QStringLiteral("manifestPath"), parser.value(manifestOption)},
        {QStringLiteral("gamesPath"), parser.value(gamesOption)},
        {QStringLiteral("picturesFolder"), parser.value(picturesOption)},
        {QStringLiteral("logOutProgram"), parser.value(logOutOption)},
        {QStringLiteral("scopeApps"), parser.isSet(scopeAppsOption)},
        {QStringLiteral("giveUpFile"), GiveUpWatch::sessionPath()},
        {QStringLiteral("steamStartedAtLogin"), startSteam},
        {QStringLiteral("batteryService"), UPowerDevice::systemService()},
    });
    engine.loadFromModule("Cairn.Launcher", "Main");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
