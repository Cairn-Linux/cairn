// SPDX-License-Identifier: Apache-2.0
#include "TileFiles.h"
#include "TileModel.h"
#include "TilePicture.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace {

QJsonObject game(const QString& title, const QString& appId) {
    return {{QStringLiteral("title"), title},
            {QStringLiteral("category"), QStringLiteral("games")},
            {QStringLiteral("exec"),
             QJsonArray{QStringLiteral("steam"), QStringLiteral("-applaunch"), appId}}};
}

QStringList titles(const QList<TileModel::Tile>& tiles) {
    QStringList out;
    for (const TileModel::Tile& tile : tiles) {
        out.append(tile.title);
    }
    return out;
}

// The manifest and the games file read together (ADR-0033). The games file
// is trusted less than the manifest: it may only start Steam games.
class TileFilesTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_manifest;

    QString write(const QString& name, const QByteArray& bytes) {
        QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return {};
        }
        file.write(bytes);
        return path;
    }

    QString writeGames(const QString& name, const QJsonArray& entries) {
        return write(name, QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                                     {QStringLiteral("entries"), entries}})
                               .toJson());
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        m_manifest = write("manifest.json", R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]},
            {"title": "Letters", "category": "practice", "exec": ["gcompris-qt"]}]})");
    }

    void noGamesFileGivesTheManifestOnly() {
        const TileFiles::Result result = TileFiles::read(m_manifest, QString());
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Letters"}));
        QVERIFY(result.manifestError.isEmpty());
        QVERIFY(result.sentences.isEmpty());
    }

    // A computer without shared games is not a fault.
    void aMissingGamesFileSaysNothing() {
        const TileFiles::Result result = TileFiles::read(m_manifest, m_dir.filePath("none.json"));
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Letters"}));
        QVERIFY(result.sentences.isEmpty());
    }

    void aGamesFileWithNoEntriesSaysNothing() {
        const QString games = writeGames("no-entries.json", {});
        const TileFiles::Result result = TileFiles::read(m_manifest, games);
        QCOMPARE(result.tiles.size(), 2);
        QVERIFY(result.sentences.isEmpty());
    }

    void gameTilesFollowTheManifestsInOrder() {
        const QString games =
            writeGames("two.json", {game("Game one", "1001"), game("Game two", "1002")});
        const TileFiles::Result result = TileFiles::read(m_manifest, games);
        QVERIFY(result.sentences.isEmpty());
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Letters", "Game one", "Game two"}));
        QCOMPARE(result.tiles.at(2).kind, TileModel::Kind::Games);
        QCOMPARE(result.tiles.at(3).exec,
                 (QStringList{QStringLiteral("steam"), QStringLiteral("-applaunch"),
                              QStringLiteral("1002")}));
        // The Terminal is TileModel's, never a file's.
        for (const TileModel::Tile& tile : result.tiles) {
            QVERIFY(!tile.opensTerminal);
        }
    }

    void aGamesFileByUrlIsRead() {
        const QString games = writeGames("url.json", {game("Game one", "1001")});
        const TileFiles::Result result =
            TileFiles::read(m_manifest, QUrl::fromLocalFile(games).toString());
        QCOMPARE(result.tiles.size(), 3);
    }

    // A games file that is there but cannot be used costs only the game
    // tiles, with one sentence for a parent; the manifest's tiles still show.
    void anUnusableGamesFileCostsOnlyTheGameTiles_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<QString>("problem");
        QTest::newRow("not JSON") << QByteArray("{ not json")
                                  << QStringLiteral("is not valid JSON");
        QTest::newRow("not an object")
            << QByteArray(R"([{"title": "Game one"}])") << QStringLiteral("is not valid JSON");
        QTest::newRow("version 2")
            << QByteArray(R"({"version": 2, "entries": [{"title": "Game one", "category": "games",
                "exec": ["steam", "-applaunch", "1001"]}]})")
            << QStringLiteral("is not version 1");
        QTest::newRow("no version")
            << QByteArray(R"({"entries": []})") << QStringLiteral("is not version 1");
    }

    void anUnusableGamesFileCostsOnlyTheGameTiles() {
        QFETCH(const QByteArray, bytes);
        QFETCH(const QString, problem);
        const QString games = write("unusable.json", bytes);
        const TileFiles::Result result = TileFiles::read(m_manifest, games);
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Letters"}));
        QVERIFY(result.manifestError.isEmpty());
        QCOMPARE(result.sentences,
                 QStringList{QStringLiteral("The games file %1 %2, so no game tiles show. "
                                            "Provisioning makes it again on its next run.")
                                 .arg(games, problem)});
    }

    void aGamesFileThatCannotBeOpenedCostsOnlyTheGameTiles() {
        // A folder where the file should be cannot be opened, whoever runs this.
        const TileFiles::Result folder = TileFiles::read(m_manifest, m_dir.path());
        QCOMPARE(folder.tiles.size(), 2);
        QCOMPARE(folder.sentences.size(), 1);
        QVERIFY(folder.sentences.at(0).contains(QStringLiteral("could not be opened")));

        const QString games = writeGames("unreadable.json", {game("Game one", "1001")});
        QVERIFY(QFile::setPermissions(games, QFileDevice::Permissions()));
        if (::access(qPrintable(games), R_OK) == 0) {
            QSKIP("This user can read a file with no permissions (root).");
        }
        const TileFiles::Result unreadable = TileFiles::read(m_manifest, games);
        QCOMPARE(unreadable.tiles.size(), 2);
        QCOMPARE(unreadable.sentences.size(), 1);
        QVERIFY(unreadable.sentences.at(0).contains(QStringLiteral("could not be opened")));
    }

    // Nothing in the games file may choose a program: only `steam`,
    // `-applaunch` and an app id, exactly.
    void aGameMayOnlyStartSteam_data() {
        QTest::addColumn<QJsonValue>("exec");
        QTest::newRow("steam by its full path") << QJsonValue(QJsonArray{
            QStringLiteral("/usr/bin/steam"), QStringLiteral("-applaunch"), QStringLiteral("7")});
        QTest::newRow("an extra argument")
            << QJsonValue(QJsonArray{QStringLiteral("steam"), QStringLiteral("-applaunch"),
                                     QStringLiteral("7"), QStringLiteral("-novid")});
        QTest::newRow("no app id")
            << QJsonValue(QJsonArray{QStringLiteral("steam"), QStringLiteral("-applaunch")});
        QTest::newRow("an app id that is not a number") << QJsonValue(QJsonArray{
            QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("seven")});
        QTest::newRow("a leading zero") << QJsonValue(QJsonArray{
            QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("07")});
        QTest::newRow("zero") << QJsonValue(
            QJsonArray{QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("0")});
        QTest::newRow("a sign") << QJsonValue(QJsonArray{
            QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("+7")});
        QTest::newRow("a line break after it") << QJsonValue(QJsonArray{
            QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("7\n")});
        QTest::newRow("eleven digits") << QJsonValue(QJsonArray{
            QStringLiteral("steam"), QStringLiteral("-applaunch"), QStringLiteral("12345678901")});
        QTest::newRow("an app id as a JSON number")
            << QJsonValue(QJsonArray{QStringLiteral("steam"), QStringLiteral("-applaunch"), 7});
        QTest::newRow("a Steam link") << QJsonValue(
            QJsonArray{QStringLiteral("steam"), QStringLiteral("steam://rungameid/7")});
        QTest::newRow("another program") << QJsonValue(QJsonArray{
            QStringLiteral("/bin/sh"), QStringLiteral("-c"), QStringLiteral("steam -applaunch 7")});
        QTest::newRow("a string, not a list") << QJsonValue(QStringLiteral("steam -applaunch 7"));
        QTest::newRow("no exec") << QJsonValue(QJsonValue::Undefined);
    }

    void aGameMayOnlyStartSteam() {
        QFETCH(const QJsonValue, exec);
        QJsonObject bad = game("Odd game", "1");
        bad.remove(QStringLiteral("exec"));
        if (!exec.isUndefined()) {
            bad.insert(QStringLiteral("exec"), exec);
        }
        const QString games = writeGames("exec.json", {bad, game("Game one", "1001")});
        const TileFiles::Result result = TileFiles::read(m_manifest, games);
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Letters", "Game one"}));
        QCOMPARE(result.sentences,
                 QStringList{QStringLiteral("The games file %1 has an entry without a title, the "
                                            "category games, or the command steam -applaunch and "
                                            "an app id: Odd game. It was left out.")
                                 .arg(games)});
    }

    void aGameNeedsATitleAndTheGamesCategory() {
        QJsonObject make = game("Made", "1002");
        make.insert(QStringLiteral("category"), QStringLiteral("make"));
        QJsonObject noCategory = game("No category", "1003");
        noCategory.remove(QStringLiteral("category"));
        const QString games = writeGames(
            "category.json", {make, noCategory, game(QString(), "1004"), game("Game one", "1001")});
        const TileFiles::Result result = TileFiles::read(m_manifest, games);
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Letters", "Game one"}));
        QCOMPARE(result.sentences.size(), 3);
        QVERIFY(result.sentences.at(0).contains(QStringLiteral(": Made. It was left out.")));
        QVERIFY(result.sentences.at(1).contains(QStringLiteral(": No category. It was left out.")));
        QVERIFY(result.sentences.at(2).contains(QStringLiteral(": (untitled). It was left out.")));
    }

    // The manifest is the Guardian's own choice, so its tile for a game
    // stays and the games file's is left out without a word.
    void aGameTheManifestHasKeepsTheManifestsTile() {
        const QString manifest = write("with-games.json", R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]},
            {"title": "Mine", "category": "games", "exec": ["steam", "-applaunch", "1001"]},
            {"title": "Full path", "category": "games",
             "exec": ["/usr/bin/steam", "-applaunch", "1002"]}]})");
        const QString games = writeGames(
            "repeats.json", {game("Game one", "1001"), game("Game two", "1002"),
                             game("Game three", "1003"), game("Game three again", "1003")});
        const TileFiles::Result result = TileFiles::read(manifest, games);
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Mine", "Full path", "Game three"}));
        QVERIFY(result.sentences.isEmpty());
    }

    // An entry left out because its game already has a tile is not checked,
    // so a parent is never told about a title for a tile that is not there.
    void aGameThatAlreadyHasATileGetsNoSentence() {
        const QString manifest = write("with-a-game.json", R"({"version": 1, "entries": [
            {"title": "Mine", "category": "games", "exec": ["steam", "-applaunch", "1001"]}]})");
        QJsonObject otherCategory = game("Game three made", "1003");
        otherCategory.insert(QStringLiteral("category"), QStringLiteral("make"));
        const QString games = writeGames(
            "already.json", {game("Game one", "1001"), game(QString(), "1001"),
                             game("Game three", "1003"), otherCategory, game(QString(), "1003")});
        const TileFiles::Result result = TileFiles::read(manifest, games);
        QCOMPARE(titles(result.tiles), (QStringList{"Mine", "Game three"}));
        QVERIFY2(result.sentences.isEmpty(), qPrintable(result.sentences.join(QLatin1Char('\n'))));
    }

    // An entry that was left out gave its game no tile, so a later entry for
    // the same game is still read, and is still checked.
    void aLaterEntryForAGameLeftOutIsStillRead() {
        QJsonObject made = game("Game one made", "1001");
        made.insert(QStringLiteral("category"), QStringLiteral("make"));
        const QString games = writeGames("second-try.json",
                                         {game(QString(), "1001"), made, game("Game one", "1001")});
        const TileFiles::Result result = TileFiles::read(m_manifest, games);
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Letters", "Game one"}));
        QCOMPARE(result.sentences.size(), 2);
        QVERIFY(result.sentences.at(0).contains(QStringLiteral(": (untitled). It was left out.")));
        QVERIFY(
            result.sentences.at(1).contains(QStringLiteral(": Game one made. It was left out.")));
    }

    // A game's picture is the picture step's copy for its app id, never one
    // the games file names, so a picture copied after the file was written
    // still shows (ADR-0047). A game without one shows its name, which is
    // not a fault, so nothing is said.
    void aGamesPictureIsFoundByItsAppId() {
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("pictures")));
        const QString pictures = m_dir.filePath(QStringLiteral("pictures"));
        const QString picture = write("pictures/1001.jpg", "stand-in picture");
        const QString named = write("named.jpg", "stand-in picture");
        QJsonObject first = game("Game one", "1001");
        first.insert(QStringLiteral("picture"), named);
        QJsonObject second = game("Game two", "1002");
        second.insert(QStringLiteral("picture"), named);
        QJsonObject third = game("Game three", "1003");
        third.insert(QStringLiteral("picture"), QStringLiteral("1003.jpg"));
        const QString games = writeGames("by-appid.json", {first, second, third});
        const TileFiles::Result result = TileFiles::read(m_manifest, games, pictures);
        QCOMPARE(result.tiles.size(), 5);
        QCOMPARE(result.tiles.at(2).picture, QUrl::fromLocalFile(picture));
        QVERIFY(result.tiles.at(3).picture.isEmpty());
        QVERIFY(result.tiles.at(4).picture.isEmpty());
        QVERIFY2(result.sentences.isEmpty(), qPrintable(result.sentences.join(QLatin1Char('\n'))));
        // The manifest's tiles keep their own rule, and get none of these.
        QVERIFY(result.tiles.at(0).picture.isEmpty());
    }

    // Only a plain file at <folder>/<appid>.jpg, with the folder a full
    // path, is a game's picture: not a link, not a folder, and nothing
    // without a folder.
    void onlyAPlainFileInAFullFolderIsAPicture_data() {
        QTest::addColumn<QString>("folder");
        QTest::addColumn<QString>("make");
        QTest::newRow("no folder") << QString() << QString();
        QTest::newRow("a relative folder") << QStringLiteral("pictures") << QString();
        QTest::newRow("the program's own") << QStringLiteral(":/pictures") << QString();
        QTest::newRow("a link") << QStringLiteral("odd") << QStringLiteral("link");
        QTest::newRow("a folder") << QStringLiteral("odd") << QStringLiteral("folder");
    }

    void onlyAPlainFileInAFullFolderIsAPicture() {
        QFETCH(const QString, folder);
        QFETCH(const QString, make);
        const QString odd = m_dir.filePath(QStringLiteral("odd"));
        QDir(odd).removeRecursively();
        QVERIFY(QDir().mkpath(odd));
        const QString elsewhere = write("elsewhere.jpg", "stand-in picture");
        if (make == QStringLiteral("link")) {
            QVERIFY(QFile::link(elsewhere, odd + QStringLiteral("/1001.jpg")));
        } else if (make == QStringLiteral("folder")) {
            QVERIFY(QDir().mkpath(odd + QStringLiteral("/1001.jpg")));
        }
        const QString games = writeGames("odd.json", {game("Game one", "1001")});
        const TileFiles::Result result =
            TileFiles::read(m_manifest, games, make.isEmpty() ? folder : odd);
        QCOMPARE(result.tiles.size(), 3);
        QVERIFY(result.tiles.at(2).picture.isEmpty());
        QVERIFY(result.sentences.isEmpty());
    }

    // The path is built from the app id, so anything but an app id gives no
    // picture, even when a file is there for it to reach.
    void aPictureNeedsAnAppId_data() {
        QTest::addColumn<QString>("appId");
        QTest::addColumn<QString>("reachable");
        QTest::newRow("up a folder") << QStringLiteral("../1001") << QStringLiteral("1001.jpg");
        QTest::newRow("through a folder")
            << QStringLiteral("1001/../1002") << QStringLiteral("pictures/1002.jpg");
        QTest::newRow("a leading zero")
            << QStringLiteral("01") << QStringLiteral("pictures/01.jpg");
        QTest::newRow("a file name")
            << QStringLiteral("1001.jpg") << QStringLiteral("pictures/1001.jpg.jpg");
        QTest::newRow("nothing") << QString() << QStringLiteral("pictures/.jpg");
    }

    void aPictureNeedsAnAppId() {
        QFETCH(const QString, appId);
        QFETCH(const QString, reachable);
        QVERIFY(QDir(m_dir.path()).mkpath(QStringLiteral("pictures/1001")));
        QVERIFY(!write(reachable, "stand-in picture").isEmpty());
        QVERIFY(TilePicture::forAppId(m_dir.filePath(QStringLiteral("pictures")), appId).isEmpty());
        // The same folder gives an app id its picture.
        write("pictures/1002.jpg", "stand-in picture");
        QCOMPARE(TilePicture::forAppId(m_dir.filePath(QStringLiteral("pictures")), "1002"),
                 QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("pictures/1002.jpg"))));
    }

    // A manifest that cannot be used gives the built-in tiles, and the games
    // still follow them.
    void builtInTilesThenGamesWhenTheManifestCannotBeUsed() {
        const QString games = writeGames("after-built-in.json", {game("Game one", "1001")});
        const TileFiles::Result result = TileFiles::read(m_dir.filePath("missing.json"), games);
        QVERIFY(!result.manifestError.isEmpty());
        QCOMPARE(result.sentences, QStringList{result.manifestError});
        QStringList expected = titles(TileModel::builtInTiles());
        expected.append(QStringLiteral("Game one"));
        QCOMPARE(titles(result.tiles), expected);
    }

    void noManifestGivesBuiltInTilesThenGames() {
        const QString games = writeGames("no-manifest.json", {game("Game one", "1001")});
        const TileFiles::Result result = TileFiles::read(QString(), games);
        QVERIFY(result.manifestError.isEmpty());
        QVERIFY(result.sentences.isEmpty());
        QCOMPARE(result.tiles.size(), TileModel::builtInTiles().size() + 1);
        QCOMPARE(result.tiles.last().title, QStringLiteral("Game one"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(TileFilesTest)
#include "tst_tilefiles.moc"
