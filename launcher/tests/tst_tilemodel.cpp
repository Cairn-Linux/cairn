// SPDX-License-Identifier: Apache-2.0
#include "TileModel.h"

#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

class TileModelTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QString write(const QString& name, const QByteArray& bytes) {
        QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return {};
        }
        file.write(bytes);
        return path;
    }

    QString title(const TileModel& model, int row) {
        return model.data(model.index(row), TileModel::TitleRole).toString();
    }

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        qmlRegisterType<TileModel>("CairnTest", 1, 0, "TileModel");
    }

    void tiles() {
        const TileModel model(this);
        const QList<TileModel::Kind> expectedKinds{
            TileModel::Kind::Make, TileModel::Kind::Make,     TileModel::Kind::Make,
            TileModel::Kind::Make, TileModel::Kind::Practice, TileModel::Kind::Machine};

        QCOMPARE(model.rowCount(), 6);
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row);
            QCOMPARE(model.data(index, TileModel::KindRole).value<TileModel::Kind>(),
                     expectedKinds.at(row));
            QVERIFY(!model.data(index, TileModel::TitleRole).toString().isEmpty());
            QVERIFY(!model.data(index, TileModel::AccessibleNameRole).toString().isEmpty());
        }
    }

    void roleNames() {
        const TileModel model(this);
        QCOMPARE(model.roleNames().value(TileModel::TitleRole), QByteArray("title"));
        QCOMPARE(model.roleNames().value(TileModel::KindRole), QByteArray("kind"));
        QCOMPARE(model.roleNames().value(TileModel::AccessibleNameRole),
                 QByteArray("accessibleName"));
        QCOMPARE(model.roleNames().value(TileModel::PictureRole), QByteArray("picture"));
    }

    // The games file's tiles come after the manifest's, and the Terminal is
    // always last (ADR-0033). A game's picture reaches QML as a file:// URL.
    void gameTilesComeBeforeTheTerminal() {
        const QString picture = write("1001.jpg", "stand-in picture");
        const QString manifest = write("manifest.json", R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]}]})");
        const QString games = write("games.json", QStringLiteral(R"({"version": 1, "entries": [
            {"title": "Game one", "category": "games", "exec": ["steam", "-applaunch", "1001"],
             "picture": "%1"},
            {"title": "Game two", "category": "games", "exec": ["steam", "-applaunch", "1002"]}]})")
                                                      .arg(picture)
                                                      .toUtf8());
        TileModel model(this);
        const QSignalSpy gamesPathChanged(&model, &TileModel::gamesPathChanged);
        model.setManifestPath(manifest);
        model.setGamesPath(games);
        QCOMPARE(model.gamesPath(), games);
        QCOMPARE(gamesPathChanged.count(), 1);
        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(title(model, 0), QStringLiteral("Paint"));
        QCOMPARE(title(model, 1), QStringLiteral("Game one"));
        QCOMPARE(title(model, 2), QStringLiteral("Game two"));
        QCOMPARE(model.data(model.index(1), TileModel::KindRole).value<TileModel::Kind>(),
                 TileModel::Kind::Games);
        QVERIFY(model.data(model.index(3), TileModel::OpensTerminalRole).toBool());
        QCOMPARE(model.data(model.index(1), TileModel::PictureRole).toUrl(),
                 QUrl::fromLocalFile(picture));
        QVERIFY(model.data(model.index(0), TileModel::PictureRole).toUrl().isEmpty());
        QVERIFY(model.data(model.index(2), TileModel::PictureRole).toUrl().isEmpty());
        QVERIFY(model.data(model.index(3), TileModel::PictureRole).toUrl().isEmpty());

        // Without the games file, the game tiles go and the Terminal stays.
        model.setGamesPath(QString());
        QCOMPARE(model.rowCount(), 2);
        QVERIFY(model.data(model.index(1), TileModel::OpensTerminalRole).toBool());
    }

    // Main.qml sets both paths at once. The files are read once, after both
    // are set, so the journal hears each sentence once and the tiles are
    // handed to the Terminal once.
    void aModelMadeInQmlReadsItsFilesOnce() {
        const QString manifest = write("qml-manifest.json", R"({"version": 1, "entries": [
            {"title": "Paint", "category": "make", "exec": ["tuxpaint"]},
            {"title": "Odd tile", "category": "toys", "exec": ["odd"]}]})");
        const QString games = write("qml-games.json", R"({"version": 1, "entries": [
            {"title": "Game one", "category": "games", "exec": ["steam", "-applaunch", "1001"]},
            {"title": "Odd game", "category": "games", "exec": ["odd"]}]})");
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(QStringLiteral("import CairnTest\n"
                                         "TileModel { manifestPath: \"%1\"; gamesPath: \"%2\" }\n")
                              .arg(manifest, games)
                              .toUtf8(),
                          QUrl());
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        QTest::failOnWarning(QRegularExpression(QStringLiteral("It was left out")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Odd tile. It was")));
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("Odd game. It was")));
        const std::unique_ptr<QObject> object(component.create());
        auto* model = qobject_cast<TileModel*>(object.get());
        QVERIFY(model != nullptr);
        QCOMPARE(model->rowCount(), 3);
        QCOMPARE(title(*model, 0), QStringLiteral("Paint"));
        QCOMPARE(title(*model, 1), QStringLiteral("Game one"));
        QVERIFY(model->data(model->index(2), TileModel::OpensTerminalRole).toBool());
    }

    void invalidRequests() {
        const TileModel model(this);
        const TileModel otherModel(this);
        QCOMPARE(model.rowCount(model.index(0)), 0);
        QVERIFY(!model.data({}, TileModel::TitleRole).isValid());
        QVERIFY(!model.data(model.index(6), TileModel::TitleRole).isValid());
        QVERIFY(!model.data(otherModel.index(0), TileModel::TitleRole).isValid());
        QVERIFY(!model.data(model.index(0), Qt::UserRole).isValid());
    }
};

} // namespace

QTEST_GUILESS_MAIN(TileModelTest)
#include "tst_tilemodel.moc"
