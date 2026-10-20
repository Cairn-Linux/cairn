// SPDX-License-Identifier: Apache-2.0
#include "TileFiles.h"
#include "TileModel.h"

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
    // so a parent is never told about a picture or a title for a tile that
    // is not there.
    void aGameThatAlreadyHasATileGetsNoSentence() {
        const QString manifest = write("with-a-game.json", R"({"version": 1, "entries": [
            {"title": "Mine", "category": "games", "exec": ["steam", "-applaunch", "1001"]}]})");
        QJsonObject missingPicture = game("Game one", "1001");
        missingPicture.insert(QStringLiteral("picture"), m_dir.filePath("no-such-1001.jpg"));
        QJsonObject relativePicture = game("Game three again", "1003");
        relativePicture.insert(QStringLiteral("picture"), QStringLiteral("1003.jpg"));
        QJsonObject otherCategory = game("Game three made", "1003");
        otherCategory.insert(QStringLiteral("category"), QStringLiteral("make"));
        const QString games = writeGames(
            "already.json", {missingPicture, game(QString(), "1001"), game("Game three", "1003"),
                             relativePicture, otherCategory, game(QString(), "1003")});
        const TileFiles::Result result = TileFiles::read(manifest, games);
        QCOMPARE(titles(result.tiles), (QStringList{"Mine", "Game three"}));
        QVERIFY2(result.sentences.isEmpty(), qPrintable(result.sentences.join(QLatin1Char('\n'))));
    }

    // An entry that was left out gave its game no tile, so a later entry for
    // the same game is still read, and is still checked.
    void aLaterEntryForAGameLeftOutIsStillRead() {
        QJsonObject relativePicture = game("Game one", "1001");
        relativePicture.insert(QStringLiteral("picture"), QStringLiteral("1001.jpg"));
        const QString games =
            writeGames("second-try.json", {game(QString(), "1001"), relativePicture});
        const TileFiles::Result result = TileFiles::read(m_manifest, games);
        QCOMPARE(titles(result.tiles), (QStringList{"Paint", "Letters", "Game one"}));
        QCOMPARE(result.sentences.size(), 2);
        QVERIFY(result.sentences.at(0).contains(QStringLiteral(": (untitled). It was left out.")));
        QVERIFY(result.sentences.at(1).contains(QStringLiteral("names a picture for Game one")));
    }

    void picturesAreCarried() {
        const QString picture = write("1001.jpg", "stand-in picture");
        QJsonObject withPicture = game("Game one", "1001");
        withPicture.insert(QStringLiteral("picture"), picture);
        QJsonObject badPicture = game("Game two", "1002");
        badPicture.insert(QStringLiteral("picture"), QStringLiteral("1002.jpg"));
        const QString games =
            writeGames("pictures.json", {withPicture, badPicture, game("Game three", "1003")});
        const TileFiles::Result result = TileFiles::read(m_manifest, games);
        QCOMPARE(result.tiles.size(), 5);
        QCOMPARE(result.tiles.at(2).picture, QUrl::fromLocalFile(picture));
        QVERIFY(result.tiles.at(3).picture.isEmpty());
        QVERIFY(result.tiles.at(4).picture.isEmpty());
        QCOMPARE(result.sentences,
                 QStringList{QStringLiteral("The games file %1 names a picture for Game two that "
                                            "is not the full path of a file: 1002.jpg. The tile "
                                            "shows its name.")
                                 .arg(games)});
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
