// SPDX-License-Identifier: Apache-2.0
#include "GamesFile.h"
#include "Manifest.h"
#include "TileModel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

namespace {

QStringList strings(const QJsonArray& array) {
    QStringList out;
    for (const auto& value : array) {
        out.append(value.toString());
    }
    return out;
}

QJsonArray entriesOf(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll())
        .object()
        .value(QStringLiteral("entries"))
        .toArray();
}

// The manifest and the games file kidscan writes, read by the launcher (#97,
// ADR-0033). CTest first runs tools/kidscan/tests/roundtrip_manifest.py,
// which scans a synthetic Steam library with a stand-in scummvm, names its
// manifest in KIDSCAN_MANIFEST, and names in KIDSCAN_GAMES the games file it
// makes from the same library as Cairn makes one, naming no picture. In
// KIDSCAN_PICTURES it names a folder with a stand-in picture for each game,
// as the picture step would leave them (ADR-0047).
class KidscanManifestTest : public QObject {
    Q_OBJECT

    QString m_path;
    QJsonArray m_entries;
    QString m_gamesPath;
    QJsonArray m_games;
    QString m_pictures;

private slots:
    void initTestCase() {
        m_path = qEnvironmentVariable("KIDSCAN_MANIFEST");
        m_gamesPath = qEnvironmentVariable("KIDSCAN_GAMES");
        m_pictures = qEnvironmentVariable("KIDSCAN_PICTURES");
        QVERIFY2(!m_path.isEmpty() && !m_gamesPath.isEmpty() && !m_pictures.isEmpty(),
                 "Run this with ctest, which writes the manifest and the games file first.");
        QVERIFY2(QFileInfo::exists(m_path), qPrintable(m_path));
        QVERIFY2(QFileInfo::exists(m_gamesPath), qPrintable(m_gamesPath));
        m_entries = entriesOf(m_path);
        m_games = entriesOf(m_gamesPath);
        QVERIFY(!m_entries.isEmpty());
        QVERIFY(!m_games.isEmpty());
    }

    // Nothing kidscan writes is left out, and nothing changes on the way.
    void everyEntryBecomesAGamesTile() {
        const Manifest::Result result = Manifest::read(m_path);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY2(result.skipped.isEmpty(), qPrintable(result.skipped.join(QLatin1Char('\n'))));
        QCOMPARE(result.tiles.size(), m_entries.size());
        for (qsizetype i = 0; i < m_entries.size(); ++i) {
            const QJsonObject entry = m_entries.at(i).toObject();
            const TileModel::Tile& tile = result.tiles.at(i);
            QCOMPARE(tile.title, entry.value(QStringLiteral("title")).toString());
            QCOMPARE(tile.kind, TileModel::Kind::Games);
            QCOMPARE(tile.exec, strings(entry.value(QStringLiteral("exec")).toArray()));
        }
    }

    // Both launch forms arrive: native ScummVM and the Steam client.
    void bothLaunchFormsArrive() {
        const Manifest::Result result = Manifest::read(m_path);
        qsizetype native = 0;
        qsizetype steam = 0;
        for (const TileModel::Tile& tile : result.tiles) {
            if (tile.exec.size() == 4 && tile.exec.at(1) == QStringLiteral("--fullscreen")) {
                QVERIFY(tile.exec.at(2).startsWith(QStringLiteral("--path=")));
                QVERIFY(tile.exec.at(3).startsWith(QStringLiteral("scumm:")));
                ++native;
            } else if (tile.exec.size() == 3 && tile.exec.at(0) == QStringLiteral("steam") &&
                       tile.exec.at(1) == QStringLiteral("-applaunch")) {
                ++steam;
            } else {
                QFAIL(qPrintable(QStringLiteral("An exec list of no known form: ") +
                                 tile.exec.join(QLatin1Char(' '))));
            }
        }
        QVERIFY(native > 0);
        QVERIFY(steam > 0);
    }

    // The games file passes the launcher's stricter rules whole: every entry
    // a Steam game, nothing left out, each with its picture found by its app
    // id, though the file names none.
    void everyGameBecomesASteamTileWithAPicture() {
        const GamesFile::Result result = GamesFile::read(m_gamesPath, {}, m_pictures);
        QVERIFY2(result.sentences.isEmpty(), qPrintable(result.sentences.join(QLatin1Char('\n'))));
        QCOMPARE(result.tiles.size(), m_games.size());
        for (qsizetype i = 0; i < m_games.size(); ++i) {
            const QJsonObject entry = m_games.at(i).toObject();
            const TileModel::Tile& tile = result.tiles.at(i);
            QCOMPARE(tile.title, entry.value(QStringLiteral("title")).toString());
            QCOMPARE(tile.kind, TileModel::Kind::Games);
            QCOMPARE(tile.exec.size(), 3);
            QCOMPARE(tile.exec.at(0), QStringLiteral("steam"));
            QCOMPARE(tile.exec.at(1), QStringLiteral("-applaunch"));
            QCOMPARE(tile.exec, strings(entry.value(QStringLiteral("exec")).toArray()));
            QVERIFY(!entry.contains(QStringLiteral("picture")));
            QVERIFY2(tile.picture.isLocalFile(), qPrintable(tile.title));
            QCOMPARE(tile.picture.toLocalFile(),
                     QDir(m_pictures).filePath(tile.exec.at(2) + QStringLiteral(".jpg")));
        }
    }
};

} // namespace

QTEST_GUILESS_MAIN(KidscanManifestTest)
#include "tst_kidscanmanifest.moc"
