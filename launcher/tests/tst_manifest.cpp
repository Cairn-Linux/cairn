// SPDX-License-Identifier: Apache-2.0
#include "Manifest.h"
#include "TileModel.h"

#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

namespace {

class ManifestTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString write(const QString& name, const QByteArray& json) {
        QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return {};
        }
        file.write(json);
        return path;
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
    }

    void readsAValidManifest() {
        const QString path = write("good.json", R"({
            "version": 1,
            "entries": [
                {"title": "Paint", "category": "make", "exec": ["tuxpaint", "--fullscreen=native"]},
                {"title": "Letters", "category": "practice", "exec": ["gcompris-qt"]},
                {"title": "Terminal", "category": "machine", "exec": []},
                {"title": "Freddi Fish", "category": "games", "exec": ["steam", "-applaunch", "1"]}
            ]
        })");
        const Manifest::Result result = Manifest::read(path);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.skipped.isEmpty());
        QCOMPARE(result.tiles.size(), 4);
        QCOMPARE(result.tiles.at(0).title, QStringLiteral("Paint"));
        QCOMPARE(result.tiles.at(0).kind, TileModel::Kind::Make);
        QCOMPARE(result.tiles.at(0).exec,
                 QStringList({QStringLiteral("tuxpaint"), QStringLiteral("--fullscreen=native")}));
        QCOMPARE(result.tiles.at(1).kind, TileModel::Kind::Practice);
        QCOMPARE(result.tiles.at(2).kind, TileModel::Kind::Machine);
        QVERIFY(result.tiles.at(2).exec.isEmpty());
        QCOMPARE(result.tiles.at(3).kind, TileModel::Kind::Games);
    }

    void acceptsAFileUrl() {
        const QString path = write("url.json",
                                   R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]}]})");
        const Manifest::Result result = Manifest::read(QUrl::fromLocalFile(path).toString());
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.tiles.size(), 1);
    }

    void missingFileIsAPlainSentence() {
        const Manifest::Result result = Manifest::read(m_dir.filePath("missing.json"));
        QVERIFY(result.tiles.isEmpty());
        QVERIFY(result.error.startsWith(QStringLiteral("Could not open the manifest")));
        QVERIFY(!result.error.contains(QStringLiteral("errno")));
    }

    void badJsonIsRefused() {
        const QString path = write("bad.json", "{ not json");
        QVERIFY(Manifest::read(path).error.contains(QStringLiteral("not valid JSON")));
    }

    void wrongVersionIsRefused() {
        const QString path = write("v2.json", R"({"version": 2, "entries": []})");
        QVERIFY(Manifest::read(path).error.contains(QStringLiteral("not version 1")));
    }

    void aBadEntryCostsOnlyItsOwnTile() {
        // An unknown category is refused, not guessed; no title or no exec
        // list is refused too. The good entries around them still load (#97).
        const QString path = write("partly.json", R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]},
            {"title": "Freddi Fish", "category": "play", "exec": ["steam", "-applaunch", "1"]},
            {"category": "make", "exec": ["nothing"]},
            {"title": "No command", "category": "games"},
            {"title": "Putt-Putt", "category": "games", "exec": ["steam", "-applaunch", "2"]}]})");
        const Manifest::Result result = Manifest::read(path);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.tiles.size(), 2);
        QCOMPARE(result.tiles.at(0).title, QStringLiteral("Paint"));
        QCOMPARE(result.tiles.at(1).title, QStringLiteral("Putt-Putt"));
        QCOMPARE(result.skipped.size(), 3);
        QVERIFY(result.skipped.at(0).contains(QStringLiteral("Freddi Fish")));
        QVERIFY(result.skipped.at(0).contains(QStringLiteral("make, practice, games or machine")));
        QVERIFY(result.skipped.at(1).contains(QStringLiteral("(untitled)")));
        QVERIFY(result.skipped.at(2).contains(QStringLiteral("No command")));
    }

    void noUsableEntryIsRefused() {
        const QString path = write("unusable.json", R"({"version": 1, "entries": [
            {"title": "Freddi Fish", "category": "play", "exec": ["steam", "-applaunch", "1"]}]})");
        const Manifest::Result result = Manifest::read(path);
        QVERIFY(result.tiles.isEmpty());
        QVERIFY(result.error.contains(QStringLiteral("no entries the launcher can use")));
        QCOMPARE(result.skipped.size(), 1);
    }

    void emptyManifestIsRefused() {
        const QString path = write("empty.json", R"({"version": 1, "entries": []})");
        QVERIFY(Manifest::read(path).error.contains(QStringLiteral("no entries")));
    }

    void modelFallsBackToDefaultsOnError() {
        TileModel model(this);
        model.setManifestPath(m_dir.filePath("missing.json"));
        QVERIFY(!model.loadError().isEmpty());
        QCOMPARE(model.rowCount(), 6);
        QCOMPARE(model.data(model.index(0), TileModel::TitleRole).toString(),
                 QStringLiteral("Draw"));
    }

    void modelUsesTheManifest() {
        const QString path = write("model.json", R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]}]})");
        TileModel model(this);
        model.setManifestPath(path);
        QVERIFY(model.loadError().isEmpty());
        // The manifest's one tile, then the launcher's own Terminal.
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), TileModel::ExecRole).toStringList(),
                 QStringList{QStringLiteral("tuxpaint")});
        QVERIFY(!model.data(model.index(0), TileModel::OpensTerminalRole).toBool());
        QVERIFY(model.data(model.index(1), TileModel::OpensTerminalRole).toBool());
        QCOMPARE(model.data(model.index(1), TileModel::TitleRole).toString(),
                 QStringLiteral("Terminal"));
        model.setManifestPath(QString());
        QCOMPARE(model.rowCount(), 6);
    }

    void modelKeepsTheGoodTilesOfAPartlyBadManifest() {
        const QString path = write("model-partly.json", R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]},
            {"title": "Odd", "category": "toys", "exec": ["odd"]}]})");
        TileModel model(this);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("Odd. It was left out")));
        model.setManifestPath(path);
        QVERIFY(model.loadError().isEmpty());
        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.data(model.index(0), TileModel::TitleRole).toString(),
                 QStringLiteral("Paint"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(ManifestTest)
#include "tst_manifest.moc"
