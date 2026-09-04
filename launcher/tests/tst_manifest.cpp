// SPDX-License-Identifier: Apache-2.0
#include "Manifest.h"
#include "TileModel.h"

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
                {"title": "Paint", "category": "make", "exec": ["tuxpaint", "--fullscreen"]},
                {"title": "Letters", "category": "practice", "exec": ["gcompris-qt"]},
                {"title": "Terminal", "category": "machine", "exec": []}
            ]
        })");
        const Manifest::Result result = Manifest::read(path);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.tiles.size(), 3);
        QCOMPARE(result.tiles.at(0).title, QStringLiteral("Paint"));
        QCOMPARE(result.tiles.at(0).kind, TileModel::Kind::Make);
        QCOMPARE(result.tiles.at(0).exec,
                 QStringList({QStringLiteral("tuxpaint"), QStringLiteral("--fullscreen")}));
        QCOMPARE(result.tiles.at(1).kind, TileModel::Kind::Practice);
        QCOMPARE(result.tiles.at(2).kind, TileModel::Kind::Machine);
        QVERIFY(result.tiles.at(2).exec.isEmpty());
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

    void unknownCategoryIsRefusedNotGuessed() {
        // kidscan writes "play"; the fourth tile kind is undecided (issue #20).
        const QString path = write("play.json", R"({"version": 1, "entries": [
            {"title": "Freddi Fish", "category": "play", "exec": ["steam", "-applaunch", "1"]}]})");
        const Manifest::Result result = Manifest::read(path);
        QVERIFY(result.tiles.isEmpty());
        QVERIFY(result.error.contains(QStringLiteral("Freddi Fish")));
        QVERIFY(result.error.contains(QStringLiteral("make, practice or machine")));
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
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), TileModel::ExecRole).toStringList(),
                 QStringList{QStringLiteral("tuxpaint")});
        model.setManifestPath(QString());
        QCOMPARE(model.rowCount(), 6);
    }
};

} // namespace

QTEST_GUILESS_MAIN(ManifestTest)
#include "tst_manifest.moc"
