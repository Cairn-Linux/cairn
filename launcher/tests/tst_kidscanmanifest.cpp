// SPDX-License-Identifier: Apache-2.0
#include "Manifest.h"
#include "TileModel.h"

#include <QFile>
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

// The manifest kidscan writes, read by the launcher (#97). CTest first runs
// tools/kidscan/tests/roundtrip_manifest.py, which scans a synthetic Steam
// library with a stand-in scummvm, and names its output in KIDSCAN_MANIFEST.
class KidscanManifestTest : public QObject {
    Q_OBJECT

    QString m_path;
    QJsonArray m_entries;

private slots:
    void initTestCase() {
        m_path = qEnvironmentVariable("KIDSCAN_MANIFEST");
        QVERIFY2(!m_path.isEmpty(), "Run this with ctest, which writes the manifest first.");
        QFile file(m_path);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(m_path));
        m_entries = QJsonDocument::fromJson(file.readAll())
                        .object()
                        .value(QStringLiteral("entries"))
                        .toArray();
        QVERIFY(!m_entries.isEmpty());
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
};

} // namespace

QTEST_GUILESS_MAIN(KidscanManifestTest)
#include "tst_kidscanmanifest.moc"
