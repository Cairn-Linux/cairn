// SPDX-License-Identifier: Apache-2.0
#include <QGuiApplication>
#include <QQmlApplicationEngine>

#include <cstdlib>

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("cairn-launcher"));
    QGuiApplication::setOrganizationName(QStringLiteral("Cairn Linux"));

    QQmlApplicationEngine engine(&application);
    engine.loadFromModule("Cairn.Launcher", "Main");
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    return QGuiApplication::exec();
}
