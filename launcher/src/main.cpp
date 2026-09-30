// SPDX-License-Identifier: Apache-2.0
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
    parser.process(application);

    QQmlApplicationEngine engine(&application);
    engine.setInitialProperties({
        {QStringLiteral("manifestPath"), parser.value(manifestOption)},
        {QStringLiteral("logOutProgram"), parser.value(logOutOption)},
    });
    engine.loadFromModule("Cairn.Launcher", "Main");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
