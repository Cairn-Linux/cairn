// SPDX-License-Identifier: Apache-2.0
#include "GiveUpWatch.h"
#include "Manifest.h"
#include "SteamAtLogin.h"

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

    const bool startSteam =
        parser.isSet(startSteamOption) &&
        SteamAtLogin::wanted(Manifest::read(parser.value(manifestOption)).tiles);
    if (startSteam) {
        SteamAtLogin::start();
    }

    QQmlApplicationEngine engine(&application);
    engine.setInitialProperties({
        {QStringLiteral("manifestPath"), parser.value(manifestOption)},
        {QStringLiteral("logOutProgram"), parser.value(logOutOption)},
        {QStringLiteral("scopeApps"), parser.isSet(scopeAppsOption)},
        {QStringLiteral("giveUpFile"), GiveUpWatch::sessionPath()},
        {QStringLiteral("steamStartedAtLogin"), startSteam},
    });
    engine.loadFromModule("Cairn.Launcher", "Main");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
