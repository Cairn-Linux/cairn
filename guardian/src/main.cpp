// SPDX-License-Identifier: Apache-2.0
#include "SteamForChild.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <cstdlib>
#include <unistd.h>

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("cairn-steam-for"));
    QGuiApplication::setOrganizationName(QStringLiteral("Cairn Linux"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Opens a child's own Steam in a window on a grown-up's desktop (ADR-0032)."));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("child"),
                                 QStringLiteral("The child's login name."));
    parser.process(application);
    if (parser.positionalArguments().size() != 1) {
        parser.showHelp(EXIT_FAILURE);
    }

    SteamForChild::Setup setup;
    setup.child = parser.positionalArguments().constFirst();
    setup.helper = QStringLiteral(CAIRN_STEAM_VISIT);
    setup.uid = getuid();
    // The menu entry for this child, so the taskbar shows its name and icon.
    QGuiApplication::setDesktopFileName(QStringLiteral("cairn-steam-for-") + setup.child);

    SteamForChild visit(setup);
    // Queued, so the window finishes what it is doing before the program ends.
    QObject::connect(&visit, &SteamForChild::quitRequested, &application, &QCoreApplication::quit,
                     Qt::QueuedConnection);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("visit"), QVariant::fromValue(&visit)}});
    engine.loadFromModule("Cairn.Guardian", "Main");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
