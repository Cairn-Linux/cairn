// SPDX-License-Identifier: Apache-2.0
#include "CatalogueRefresh.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

namespace {

constexpr auto twoGames = R"({
  "version": 1,
  "steam_roots": ["/var/lib/cairn/steam-library"],
  "scummvm": null,
  "entries": [
    {"id": "steam-620", "title": "Portal 2", "category": "games", "engine": "steam",
     "exec": ["steam", "-applaunch", "620"]},
    {"id": "steam-12", "title": "Zoo Tycoon", "category": "games", "engine": "steam",
     "exec": ["steam", "-applaunch", "12"]}
  ]
}
)";

constexpr auto oneGame = R"({"version": 1, "entries": [{"id": "steam-7", "title": "Old",
  "category": "games", "engine": "steam", "exec": ["steam", "-applaunch", "7"]}]}
)";

class CatalogueRefreshTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    void give(const QString& name, const QByteArray& text) const {
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(text);
    }

    QByteArray read(const QString& name) const {
        QFile file(path(name));
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    QStringList lines(const QString& name) const {
        return QString::fromUtf8(read(name)).split('\n', Qt::SkipEmptyParts);
    }

    QStringList pythonCalls() const {
        return lines("python-calls");
    }

    QStringList calls() const {
        return lines("calls");
    }

    CatalogueRefresh::Setup setup() const {
        CatalogueRefresh::Setup setup;
        setup.python = path("python");
        setup.pictureStep = QStringLiteral("/x/cairn-game-pictures");
        setup.kidscan = QStringLiteral("/x/kidscan.py");
        setup.library = path("library");
        setup.pictures = path("pictures");
        setup.gamesFile = path("games.json");
        setup.restorecon = path("restorecon");
        return setup;
    }

    QTemporaryDir m_temporary;

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        for (const char* name : {"python", "restorecon"}) {
            const QString fixture = QStringLiteral(FIXTURES "/") +
                                    (QByteArray(name) == "python" ? "python" : "system-tool");
            QFile::remove(path(name));
            QVERIFY(QFile::link(fixture, path(name)));
        }
        for (const char* name : {"python-calls", "calls", "games.json", "fail-pictures",
                                 "fail-kidscan", "python-environment"}) {
            QFile::remove(path(name));
        }
        give("kidscan-output", twoGames);
    }

    // The pictures come from the home the caller check found, whatever
    // $HOME says, then kidscan reads only the shared folder, and its list
    // goes into the games file byte for byte, for everyone to read.
    void makesBoth() {
        qputenv("HOME", "/home/someone-else");
        const CatalogueRefresh::Result result =
            CatalogueRefresh::run(setup(), "/var/home/gina", path("games"));
        QVERIFY(result.picturesMade);
        QVERIFY(result.catalogueMade);
        QVERIFY(result.catalogue.has_value());
        QVERIFY(result.catalogue.value_or(GameCatalogue()).has("620"));
        QCOMPARE(pythonCalls(),
                 QStringList({"-I -B /x/cairn-game-pictures --home /var/home/gina --shared " +
                                  path("library") + " --pictures " + path("pictures") +
                                  " --lists " + path("games"),
                              "-I -B /x/kidscan.py --no-scummvm --library " + path("library") +
                                  " --pictures " + path("pictures")}));
        QCOMPARE(read("games.json"), QByteArray(twoGames));
        struct stat status{};
        QVERIFY(stat(QFile::encodeName(path("games.json")).constData(), &status) == 0);
        QCOMPARE(status.st_mode & 0777, mode_t{0644});
        QCOMPARE(calls(), QStringList({"restorecon " + path("games.json")}));
        const QString environment = QString::fromUtf8(read("python-environment"));
        QVERIFY(environment.contains("PATH=/usr/sbin:/usr/bin\n"));
        QVERIFY(!environment.contains("someone-else"));
        QVERIFY(!environment.contains("PKEXEC_UID"));
    }

    // The same list again changes nothing and labels nothing.
    void leavesTheSameListAlone() {
        give("games.json", twoGames);
        QVERIFY(CatalogueRefresh::run(setup(), "/var/home/gina", path("games")).catalogueMade);
        QVERIFY(calls().isEmpty());
    }

    // Under sudo there is no Guardian's Steam, so the pictures stay.
    void sudoKeepsThePictures() {
        const CatalogueRefresh::Result result =
            CatalogueRefresh::run(setup(), std::nullopt, path("games"));
        QVERIFY(!result.picturesMade);
        QVERIFY(result.catalogueMade);
        QCOMPARE(pythonCalls().size(), 1);
        QVERIFY(pythonCalls().constFirst().contains("kidscan.py"));
    }

    // The list is still made when the pictures are not.
    void aPictureFailureStillMakesTheList() {
        give("fail-pictures", "");
        const CatalogueRefresh::Result result =
            CatalogueRefresh::run(setup(), "/var/home/gina", path("games"));
        QVERIFY(!result.picturesMade);
        QVERIFY(result.catalogueMade);
    }

    void keepsTheOldList_data() {
        QTest::addColumn<QByteArray>("output");
        QTest::addColumn<bool>("fails");
        QTest::newRow("kidscan fails") << QByteArray(twoGames) << true;
        QTest::newRow("not json") << QByteArray("tiles: 2") << false;
        QTest::newRow("version 2") << QByteArray(R"({"version": 2, "entries": []})") << false;
        QTest::newRow("no entries") << QByteArray(R"({"version": 1})") << false;
        QTest::newRow("nothing") << QByteArray() << false;
    }

    // Anything but a games file leaves the old one, which is read instead.
    void keepsTheOldList() {
        QFETCH(const QByteArray, output);
        QFETCH(const bool, fails);
        give("games.json", oneGame);
        give("kidscan-output", output);
        if (fails) {
            give("fail-kidscan", "");
        }
        const CatalogueRefresh::Result result =
            CatalogueRefresh::run(setup(), "/var/home/gina", path("games"));
        QVERIFY(!result.catalogueMade);
        QCOMPARE(read("games.json"), QByteArray(oneGame));
        QVERIFY(result.catalogue.value_or(GameCatalogue()).has("7"));
        QVERIFY(calls().isEmpty());
    }

    // With no list at all no game can be added.
    void noListAtAll() {
        give("fail-kidscan", "");
        QVERIFY(
            !CatalogueRefresh::run(setup(), "/var/home/gina", path("games")).catalogue.has_value());
    }
};

} // namespace

QTEST_GUILESS_MAIN(CatalogueRefreshTest)
#include "tst_cataloguerefresh.moc"
