// SPDX-License-Identifier: Apache-2.0
#include "Manifest.h"
#include "TileModel.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

    // A picture the manifest names is a file on this computer, by its full
    // path (ADR-0035). The stand-in is any bytes: only the launcher's window
    // ever decodes it.
    void aPictureIsAFullPathToAFile() {
        const QString picture = write("picture.jpg", "stand-in picture");
        const QJsonObject entry{{QStringLiteral("title"), QStringLiteral("Paint")},
                                {QStringLiteral("category"), QStringLiteral("make")},
                                {QStringLiteral("exec"), QJsonArray{QStringLiteral("tuxpaint")}},
                                {QStringLiteral("picture"), picture}};
        const QString path =
            write("picture.json",
                  QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                            {QStringLiteral("entries"), QJsonArray{entry}}})
                      .toJson());
        const Manifest::Result result = Manifest::read(path);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.skipped.isEmpty());
        QVERIFY2(result.notes.isEmpty(), qPrintable(result.notes.join(QLatin1Char('\n'))));
        QCOMPARE(result.tiles.size(), 1);
        QCOMPARE(result.tiles.at(0).picture, QUrl::fromLocalFile(picture));
    }

    void noPictureSaysNothing() {
        const QString path = write("no-picture.json", R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]}]})");
        const Manifest::Result result = Manifest::read(path);
        QCOMPARE(result.tiles.size(), 1);
        QVERIFY(result.tiles.at(0).picture.isEmpty());
        QVERIFY(result.notes.isEmpty());
    }

    // A picture the launcher cannot use costs the picture, never the tile.
    void anUnusablePictureKeepsTheTile_data() {
        QTest::addColumn<QJsonValue>("picture");
        QTest::addColumn<QString>("named");
        QTest::newRow("a relative path")
            << QJsonValue(QStringLiteral("picture.jpg")) << QStringLiteral("picture.jpg");
        QTest::newRow("a web address") << QJsonValue(QStringLiteral("http://localhost/p.jpg"))
                                       << QStringLiteral("http://localhost/p.jpg");
        QTest::newRow("a file URL") << QJsonValue(QStringLiteral("file:///tmp/p.jpg"))
                                    << QStringLiteral("file:///tmp/p.jpg");
        QTest::newRow("a file that is not there")
            << QJsonValue(QStringLiteral("/nonexistent/cairn/p.jpg"))
            << QStringLiteral("/nonexistent/cairn/p.jpg");
        QTest::newRow("a folder") << QJsonValue(QDir::rootPath()) << QDir::rootPath();
        QTest::newRow("inside the program")
            << QJsonValue(QStringLiteral(":/qt-project.org")) << QStringLiteral(":/qt-project.org");
        QTest::newRow("empty") << QJsonValue(QString()) << QStringLiteral("(empty)");
        QTest::newRow("not a string") << QJsonValue(7) << QStringLiteral("(not text)");
    }

    void anUnusablePictureKeepsTheTile() {
        QFETCH(const QJsonValue, picture);
        QFETCH(const QString, named);
        const QJsonObject entry{{QStringLiteral("title"), QStringLiteral("Paint")},
                                {QStringLiteral("category"), QStringLiteral("make")},
                                {QStringLiteral("exec"), QJsonArray{QStringLiteral("tuxpaint")}},
                                {QStringLiteral("picture"), picture}};
        const QString path =
            write("unusable-picture.json",
                  QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
                                            {QStringLiteral("entries"), QJsonArray{entry}}})
                      .toJson());
        const Manifest::Result result = Manifest::read(path);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.skipped.isEmpty());
        QCOMPARE(result.tiles.size(), 1);
        QCOMPARE(result.tiles.at(0).title, QStringLiteral("Paint"));
        QVERIFY(result.tiles.at(0).picture.isEmpty());
        QCOMPARE(result.notes,
                 QStringList{QStringLiteral("The manifest %1 names a picture for Paint that is not "
                                            "the full path of a file: %2. The tile shows its name.")
                                 .arg(path, named)});
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

    // The manifest the image and provisioning install. Draw's options come
    // after the app id, where flatpak hands them to Tux Paint: --autosave and
    // --saveovernew keep the picture when the child leaves (ADR-0019), and
    // --noprint turns off a Print button its sandbox can never use (#221).
    // Tux Paint quits at once on an option it does not know, so the whole
    // command is pinned.
    void installedDrawStartsTuxPaintWithPrintOff() {
        const Manifest::Result result = Manifest::read(QStringLiteral(CAIRN_MANIFEST_JSON));
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY2(result.skipped.isEmpty(), qPrintable(result.skipped.join(QLatin1Char('\n'))));
        QStringList drawExec;
        for (const TileModel::Tile& tile : result.tiles) {
            if (tile.title == QStringLiteral("Draw")) {
                drawExec = tile.exec;
            }
        }
        QCOMPARE(drawExec,
                 (QStringList{QStringLiteral("flatpak"), QStringLiteral("run"),
                              QStringLiteral("org.tuxpaint.Tuxpaint"),
                              QStringLiteral("--fullscreen=native"), QStringLiteral("--autosave"),
                              QStringLiteral("--saveovernew"), QStringLiteral("--noprint")}));
    }
};

} // namespace

QTEST_GUILESS_MAIN(ManifestTest)
#include "tst_manifest.moc"
