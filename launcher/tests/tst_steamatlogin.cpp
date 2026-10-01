// SPDX-License-Identifier: Apache-2.0
#include "SteamAtLogin.h"
#include "TileModel.h"

#include <QTest>

namespace {

TileModel::Tile tile(const QString& title, const QStringList& exec) {
    return {.title = title, .kind = TileModel::Kind::Games, .exec = exec, .opensTerminal = false};
}

class SteamAtLoginTest : public QObject {
    Q_OBJECT

private slots:
    // Only a child with a Steam game pays for the client's memory (ADR-0027).
    void aSteamGameTileWantsSteam_data() {
        QTest::addColumn<QStringList>("exec");
        QTest::addColumn<bool>("wanted");
        QTest::newRow("steam -applaunch")
            << QStringList{QStringLiteral("steam"), QStringLiteral("-applaunch"),
                           QStringLiteral("283920")}
            << true;
        QTest::newRow("by its full path")
            << QStringList{QStringLiteral("/usr/bin/steam"), QStringLiteral("-applaunch"),
                           QStringLiteral("283920")}
            << true;
        QTest::newRow("native ScummVM")
            << QStringList{QStringLiteral("/usr/bin/scummvm"), QStringLiteral("--fullscreen"),
                           QStringLiteral("scumm:puttputt")}
            << false;
        QTest::newRow("a Flatpak app")
            << QStringList{QStringLiteral("flatpak"), QStringLiteral("run"),
                           QStringLiteral("org.tuxpaint.Tuxpaint")}
            << false;
        QTest::newRow("Steam, but not a game")
            << QStringList{QStringLiteral("steam"), QStringLiteral("steam://open/main")} << false;
        QTest::newRow("a program that only mentions Steam")
            << QStringList{QStringLiteral("echo"), QStringLiteral("steam"),
                           QStringLiteral("-applaunch")}
            << false;
        QTest::newRow("nothing set up") << QStringList{} << false;
    }

    void aSteamGameTileWantsSteam() {
        QFETCH(const QStringList, exec);
        QFETCH(const bool, wanted);
        const QList<TileModel::Tile> tiles{
            tile(QStringLiteral("Draw"),
                 {QStringLiteral("flatpak"), QStringLiteral("run"), QStringLiteral("tuxpaint")}),
            tile(QStringLiteral("Game"), exec)};
        QCOMPARE(SteamAtLogin::wanted(tiles), wanted);
    }

    void noTilesWantsNothing() {
        QVERIFY(!SteamAtLogin::wanted({}));
    }

    // Its own service in the child's systemd, on the child's display, quiet.
    void theClientStartsAsAServiceOfItsOwn() {
        QCOMPARE(SteamAtLogin::command(),
                 (QStringList{QStringLiteral("systemd-run"), QStringLiteral("--user"),
                              QStringLiteral("--unit=cairn-steam"), QStringLiteral("--collect"),
                              QStringLiteral("--quiet"), QStringLiteral("--setenv=DISPLAY"),
                              QStringLiteral("--setenv=WAYLAND_DISPLAY"), QStringLiteral("--"),
                              QStringLiteral("steam"), QStringLiteral("-silent")}));
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamAtLoginTest)
#include "tst_steamatlogin.moc"
