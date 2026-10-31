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

constexpr auto threeGames = R"({"version": 1, "entries": [
  {"title": "Portal 2", "category": "games", "exec": ["steam", "-applaunch", "620"]},
  {"title": "Old", "category": "games", "exec": ["steam", "-applaunch", "7"]},
  {"title": "Zoo Tycoon", "category": "games", "exec": ["steam", "-applaunch", "12"]}]}
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

    CatalogueRefresh::Result runAsGina() const {
        return CatalogueRefresh::run(setup(), "/var/home/gina", path("games"));
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
                                 "fail-kidscan", "python-environment", "pictures-output"}) {
            QFile::remove(path(name));
        }
        QDir(path("games.json")).removeRecursively();
        give("kidscan-output", twoGames);
        // Portal 2 has its picture; Zoo Tycoon has none.
        QDir(path("pictures")).removeRecursively();
        QVERIFY(QDir().mkpath(path("pictures")));
        give("pictures/620.jpg", "jpeg");
    }

    // The pictures come from the home the caller check found, whatever
    // $HOME says, keeping the games on the lists in `lists` (ADR-0047),
    // then kidscan reads only the shared folder, and its list goes into the
    // games file byte for byte, for everyone to read. It names no picture:
    // a tile finds one by its app id.
    void makesBoth() {
        qputenv("HOME", "/home/someone-else");
        const CatalogueRefresh::Result result = runAsGina();
        QVERIFY(result.picturesMade);
        QVERIFY(result.catalogueMade);
        QVERIFY(result.catalogue.has_value());
        QVERIFY(result.catalogue.value_or(GameCatalogue()).has("620"));
        QCOMPARE(pythonCalls(),
                 QStringList({"-I -B /x/cairn-game-pictures --home /var/home/gina --shared " +
                                  path("library") + " --pictures " + path("pictures") +
                                  " --lists " + path("games"),
                              "-I -B /x/kidscan.py --no-scummvm --library " + path("library")}));
        QCOMPARE(read("games.json"), QByteArray(twoGames));
        struct stat status{};
        QVERIFY(stat(QFile::encodeName(path("games.json")).constData(), &status) == 0);
        QCOMPARE(status.st_mode & 0777, mode_t{0644});
        QCOMPARE(calls(), QStringList({"restorecon " + path("games.json")}));
        const QString environment = QString::fromUtf8(read("python-environment"));
        QVERIFY(environment.contains("PATH=/usr/sbin:/usr/bin\n"));
        QVERIFY(!environment.contains("someone-else"));
        QVERIFY(!environment.contains("PKEXEC_UID"));
        // Nothing for the journal, and the one game whose tile shows its
        // name for want of a picture.
        QVERIFY(result.pictureNotes.isEmpty());
        QCOMPARE(result.catalogueNote, QString());
        QCOMPARE(result.withoutPicture, QStringList({"picture-missing 12"}));
    }

    // The same list again changes nothing and labels nothing.
    void leavesTheSameListAlone() {
        give("games.json", twoGames);
        QVERIFY(runAsGina().catalogueMade);
        QVERIFY(calls().isEmpty());
    }

    // Each of the picture step's own lines goes to the journal, and a game
    // whose picture it left out is named apart from one its Steam has none
    // for, since opening Steam does not help it.
    void thePictureStepsLinesAreKept() {
        give("pictures-output", "copied 620\nmissing 12\nrefused 7 not-a-jpeg\nfinished\n");
        give("kidscan-output", threeGames);
        const CatalogueRefresh::Result result = runAsGina();
        QVERIFY(result.picturesMade);
        QCOMPARE(result.pictureNotes,
                 QStringList({"copied 620", "missing 12", "refused 7 not-a-jpeg"}));
        QCOMPARE(result.picturesRefused, QStringList({"7"}));
        QCOMPARE(result.withoutPicture, QStringList({"picture-refused 7", "picture-missing 12"}));
    }

    // A picture the step left out, while an older one stays in place, still
    // shows on the tile, so the game is not named.
    void aPictureLeftOutWithOneInPlaceIsNotNamed() {
        give("pictures-output", "refused 620 too-big\nfinished\n");
        QCOMPARE(runAsGina().withoutPicture, QStringList({"picture-missing 12"}));
    }

    // Only a plain file is a picture, by the launcher's rule.
    void aFolderInAPicturesPlaceIsNoPicture() {
        QVERIFY(QFile::remove(path("pictures/620.jpg")));
        QVERIFY(QDir().mkpath(path("pictures/620.jpg")));
        QCOMPARE(runAsGina().withoutPicture,
                 QStringList({"picture-missing 620", "picture-missing 12"}));
    }

    // Only the step's own words, with an appid and a reason word as
    // cairn-family checks them; anything else is left out.
    void takePictureLineTakesOnlyTheStepsWords_data() {
        QTest::addColumn<QString>("line");
        QTest::addColumn<bool>("taken");
        QTest::newRow("copied") << "copied 620" << true;
        QTest::newRow("removed") << "removed 12" << true;
        QTest::newRow("missing") << "missing 504230" << true;
        QTest::newRow("refused") << "refused 620 not-a-jpeg" << true;
        QTest::newRow("a leading zero") << "copied 0620" << false;
        QTest::newRow("a path after it") << "copied 620 /home/gina" << false;
        QTest::newRow("a reason in capitals") << "refused 620 Not-A-Jpeg" << false;
        QTest::newRow("no reason") << "refused 620" << false;
        QTest::newRow("words after the reason") << "refused 620 not-a-jpeg now" << false;
        QTest::newRow("a path") << "/home/gina" << false;
        QTest::newRow("no appid") << "missing" << false;
        QTest::newRow("words after an ending") << "refused folder now" << false;
        QTest::newRow("finished in capitals") << "Finished" << false;
        QTest::newRow("an escape") << "\x1b[2J" << false;
        QTest::newRow("eleven digits") << "copied 12345678901" << false;
        QTest::newRow("empty") << "" << false;
    }

    void takePictureLineTakesOnlyTheStepsWords() {
        QFETCH(const QString, line);
        QFETCH(const bool, taken);
        CatalogueRefresh::Result result;
        CatalogueRefresh::takePictureLine(result, line);
        QCOMPARE(result.pictureNotes, taken ? QStringList{line} : QStringList());
        QVERIFY(!result.picturesMade);
        QVERIFY(!result.picturesEndedEarly);
        const bool leftOut = taken && line.startsWith(QStringLiteral("refused "));
        QCOMPARE(result.picturesRefused, leftOut ? QStringList{"620"} : QStringList());
    }

    // The step said why it did not go to its end. Its own word is enough,
    // the pictures stay as they were, and no game is named.
    void aStepThatSaysWhyItEndedEarly_data() {
        QTest::addColumn<QString>("line");
        QTest::newRow("failed") << "failed";
        QTest::newRow("refused folder") << "refused folder";
        QTest::newRow("refused lists") << "refused lists";
    }

    void aStepThatSaysWhyItEndedEarly() {
        QFETCH(const QString, line);
        give("pictures-output", (line + "\n").toUtf8());
        const CatalogueRefresh::Result result = runAsGina();
        QVERIFY(!result.picturesMade);
        QVERIFY(result.picturesEndedEarly);
        QCOMPARE(result.pictureNotes, QStringList{line});
        QVERIFY(result.withoutPicture.isEmpty());
        QVERIFY(result.catalogueMade);
    }

    // A step that did not say why it stopped gets a word of cairn-family's
    // own, so the journal always says why: no line at all, or some lines
    // and then nothing more. A line cut off is not a line, and more than
    // the step ever says is none of its own.
    void aStepThatStopsWithoutAWord_data() {
        QTest::addColumn<QByteArray>("output");
        QTest::addColumn<bool>("fails");
        QTest::addColumn<QStringList>("notes");
        QTest::newRow("nothing") << QByteArray() << true << QStringList{"no-answer"};
        QTest::newRow("some lines")
            << QByteArray("copied 620\n") << false << QStringList{"copied 620", "stopped"};
        QTest::newRow("finished cut off")
            << QByteArray("copied 620\nfinished") << false << QStringList{"copied 620", "stopped"};
        QTest::newRow("more than it says")
            << QByteArray("copied 620\n").repeated(100000) + "finished\n"
            << false << QStringList{"no-answer"};
    }

    void aStepThatStopsWithoutAWord() {
        QFETCH(const QByteArray, output);
        QFETCH(const bool, fails);
        QFETCH(const QStringList, notes);
        give("pictures-output", output);
        if (fails) {
            give("fail-pictures", "");
        }
        const CatalogueRefresh::Result result = runAsGina();
        QVERIFY(!result.picturesMade);
        QCOMPARE(result.pictureNotes, notes);
        QVERIFY(result.withoutPicture.isEmpty());
        QVERIFY(result.catalogueMade);
    }

    // Under sudo there is no Guardian's Steam, so the pictures stay, and
    // the journal says why.
    void sudoKeepsThePictures() {
        const CatalogueRefresh::Result result =
            CatalogueRefresh::run(setup(), std::nullopt, path("games"));
        QVERIFY(!result.picturesMade);
        QVERIFY(result.catalogueMade);
        QCOMPARE(pythonCalls().size(), 1);
        QVERIFY(pythonCalls().constFirst().contains("kidscan.py"));
        QCOMPARE(result.pictureNotes, QStringList({"no-home"}));
        QVERIFY(result.withoutPicture.isEmpty());
    }

    void keepsTheOldList_data() {
        QTest::addColumn<QByteArray>("output");
        QTest::addColumn<bool>("fails");
        QTest::addColumn<QString>("note");
        QTest::newRow("kidscan fails") << QByteArray(twoGames) << true << "failed";
        QTest::newRow("not json") << QByteArray("tiles: 2") << false << "not-a-games-list";
        QTest::newRow("version 2")
            << QByteArray(R"({"version": 2, "entries": []})") << false << "not-a-games-list";
        QTest::newRow("no entries")
            << QByteArray(R"({"version": 1})") << false << "not-a-games-list";
        QTest::newRow("nothing") << QByteArray() << false << "not-a-games-list";
    }

    // Anything but a games file leaves the old one, which is read instead,
    // and the journal gets the reason. Its games are named as any others.
    void keepsTheOldList() {
        QFETCH(const QByteArray, output);
        QFETCH(const bool, fails);
        QFETCH(const QString, note);
        give("games.json", oneGame);
        give("kidscan-output", output);
        if (fails) {
            give("fail-kidscan", "");
        }
        const CatalogueRefresh::Result result = runAsGina();
        QVERIFY(!result.catalogueMade);
        QCOMPARE(result.catalogueNote, note);
        QCOMPARE(read("games.json"), QByteArray(oneGame));
        QVERIFY(result.catalogue.value_or(GameCatalogue()).has("7"));
        QVERIFY(calls().isEmpty());
        QCOMPARE(result.withoutPicture, QStringList({"picture-missing 7"}));
    }

    // A games file that cannot be put in place is said apart from a list
    // kidscan could not make.
    void aListThatCannotBeWritten() {
        QVERIFY(QDir().mkpath(path("games.json")));
        const CatalogueRefresh::Result result = runAsGina();
        QVERIFY(!result.catalogueMade);
        QCOMPARE(result.catalogueNote, QStringLiteral("not-written"));
        QVERIFY(!result.catalogue.has_value());
        QVERIFY(result.withoutPicture.isEmpty());
    }

    // With no list at all no game can be added.
    void noListAtAll() {
        give("fail-kidscan", "");
        QVERIFY(!runAsGina().catalogue.has_value());
    }
};

} // namespace

QTEST_GUILESS_MAIN(CatalogueRefreshTest)
#include "tst_cataloguerefresh.moc"
