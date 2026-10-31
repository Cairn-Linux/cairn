// SPDX-License-Identifier: Apache-2.0
#include "SteamAtLogin.h"
#include "TileFiles.h"
#include "TileModel.h"

#include <QFile>
#include <QTemporaryDir>
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

    // main.cpp asks with the tiles of both files. A child whose only Steam
    // games come from the games file still gets Steam at login (ADR-0033).
    void gamesFromTheGamesFileWantSteam() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString manifest = dir.filePath(QStringLiteral("manifest.json"));
        const QString games = dir.filePath(QStringLiteral("games.json"));
        QFile manifestFile(manifest);
        QVERIFY(manifestFile.open(QIODevice::WriteOnly));
        manifestFile.write(R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]}]})");
        manifestFile.close();
        QVERIFY(!SteamAtLogin::wanted(TileFiles::read(manifest, games).tiles));

        QFile gamesFile(games);
        QVERIFY(gamesFile.open(QIODevice::WriteOnly));
        gamesFile.write(R"({"version": 1, "entries": [
            {"title": "Game one", "category": "games", "exec": ["steam", "-applaunch", "1001"]}]})");
        gamesFile.close();
        QVERIFY(SteamAtLogin::wanted(TileFiles::read(manifest, games).tiles));
        QVERIFY(SteamAtLogin::wanted(TileFiles::read(QString(), games).tiles));
    }

    // On the image each child reads their own list, which a Guardian's Save
    // writes (ADR-0041). A child with none yet has no game tiles, no
    // sentence for the parent, and no Steam at login.
    void aChildWithNoListGetsNoSteam() {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString manifest = dir.filePath(QStringLiteral("manifest.json"));
        QFile manifestFile(manifest);
        QVERIFY(manifestFile.open(QIODevice::WriteOnly));
        manifestFile.write(R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]}]})");
        manifestFile.close();
        const TileFiles::Result result =
            TileFiles::read(manifest, dir.filePath(QStringLiteral("games/ada.json")));
        QCOMPARE(result.tiles.size(), 1);
        QVERIFY(result.sentences.isEmpty());
        QVERIFY(!SteamAtLogin::wanted(result.tiles));
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

    // The launcher starts Steam again under the same name, and only ever
    // stops the service of that name (#206).
    void theServiceHasOneName() {
        QCOMPARE(SteamAtLogin::unit(), QStringLiteral("cairn-steam"));
        QVERIFY(SteamAtLogin::command().contains(QStringLiteral("--unit=") + SteamAtLogin::unit()));
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamAtLoginTest)
#include "tst_steamatlogin.moc"
