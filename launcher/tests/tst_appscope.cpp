// SPDX-License-Identifier: Apache-2.0
#include "AppScope.h"

#include <QCoreApplication>
#include <QTest>

namespace {

class AppScopeTest : public QObject {
    Q_OBJECT

private slots:
    void aScopeIsNamedForTheLauncherAndTheLaunch() {
        QCOMPARE(AppScope::unitName(1234, 7), QStringLiteral("cairn-app-1234-7"));
    }

    // cairn-give-up ends cairn-app-*.scope (ADR-0025), so every program runs
    // in one, under the user's own systemd, and is the scope's process.
    void theProgramRunsInANewUserScope() {
        const QStringList command =
            AppScope::command({QStringLiteral("scummvm"), QStringLiteral("--fullscreen")}, 3);
        const QString unit = AppScope::unitName(QCoreApplication::applicationPid(), 3);
        QCOMPARE(command, (QStringList{QStringLiteral("systemd-run"), QStringLiteral("--user"),
                                       QStringLiteral("--scope"), QStringLiteral("--quiet"),
                                       QStringLiteral("--collect"),
                                       QStringLiteral("--unit=") + unit, QStringLiteral("--"),
                                       QStringLiteral("scummvm"), QStringLiteral("--fullscreen")}));
    }

    void aProgramNamedLikeAnOptionIsStillTheProgram() {
        const QStringList command = AppScope::command({QStringLiteral("-odd")}, 1);
        QCOMPARE(command.at(command.size() - 2), QStringLiteral("--"));
        QCOMPARE(command.last(), QStringLiteral("-odd"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(AppScopeTest)
#include "tst_appscope.moc"
