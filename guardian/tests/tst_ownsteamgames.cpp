// SPDX-License-Identifier: Apache-2.0
#include "OwnSteamGames.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

constexpr const char* kLibraries = FIXTURES "/steam-libraries";

// A Guardian's home in a temporary folder, with the fixture libraries:
// "own" as Steam's own library, "other" on a second drive and "shared" as
// the family's shared folder.
class OwnSteamGamesTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    static void write(const QString& path, const QByteArray& bytes) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    // The fixture library `name` copied to `steamapps`.
    static void copyLibrary(const QString& name, const QString& steamapps) {
        QVERIFY(QDir().mkpath(steamapps));
        const QDir from(QString::fromUtf8(kLibraries) + u'/' + name);
        for (const QString& file : from.entryList(QDir::Files)) {
            QVERIFY(QFile::copy(from.filePath(file), QDir(steamapps).filePath(file)));
        }
    }

    QString home() const {
        return path("home");
    }

    QString shared() const {
        return path("steam-library");
    }

    QString steamapps() const {
        return home() + "/.local/share/Steam/steamapps";
    }

    // Steam's list of its libraries: its own, the second drive and the
    // shared folder, as Steam writes it once the Guardian adds them.
    QByteArray libraryFolders() const {
        return QString(R"("libraryfolders"
{
	"0"
	{
		"path"		"%1"
		"label"		""
		"apps"
		{
			"620"		"1"
		}
	}
	"1"
	{
		"path"		"%2"
	}
	"2"
	{
		"path"		"%3"
	}
}
)")
            .arg(home() + "/.local/share/Steam", path("drive"), shared())
            .toUtf8();
    }

    static QStringList names(const QList<SteamManifest::Game>& games) {
        return OwnSteamGames::names(games);
    }

    QTemporaryDir m_temporary;

private slots:
    void init() {
        QVERIFY(QDir(m_temporary.path()).removeRecursively());
        QVERIFY(QDir().mkpath(home()));
        copyLibrary("own", steamapps());
        copyLibrary("other", path("drive/steamapps"));
        copyLibrary("shared", shared() + "/steamapps");
        // The links Steam makes in a home, which name the same library.
        QVERIFY(QDir().mkpath(home() + "/.steam"));
        QVERIFY(QFile::link(home() + "/.local/share/Steam", home() + "/.steam/steam"));
        QVERIFY(QFile::link(home() + "/.local/share/Steam", home() + "/.steam/root"));
    }

    // Steam's own library only: its runtimes, Proton and a manifest cut
    // short are left out, and a game still downloading is named.
    void theGamesInSteamsOwnLibrary() {
        QCOMPARE(names(OwnSteamGames::find(home(), shared(), {})),
                 QStringList({"Celeste", "Portal 2"}));
    }

    void everyLibraryButTheSharedFolder() {
        write(steamapps() + "/libraryfolders.vdf", libraryFolders());
        const QList<SteamManifest::Game> games = OwnSteamGames::find(home(), shared(), {});
        QCOMPARE(names(games), QStringList({"Celeste", "Half-Life 2", "Portal 2"}));
        QCOMPARE(games.at(2).appId, QString("620"));
    }

    void oneSteamWhateverItsLinks() {
        QCOMPARE(OwnSteamGames::steamRoots(home()),
                 QStringList({QDir(home() + "/.local/share/Steam").canonicalPath()}));
    }

    void theFlatpaksSteamToo() {
        copyLibrary("other", home() + "/.var/app/com.valvesoftware.Steam/data/Steam/steamapps");
        QCOMPARE(names(OwnSteamGames::find(home(), shared(), {})),
                 QStringList({"Celeste", "Half-Life 2", "Portal 2"}));
    }

    // A game the family already has is not missing from it.
    void gamesTheFamilyHasAreLeftOut() {
        QCOMPARE(names(OwnSteamGames::find(home(), shared(), {"620"})), QStringList({"Celeste"}));
    }

    // Reached through a link, the shared folder is still the shared
    // folder.
    void theSharedFolderThroughALink() {
        write(steamapps() + "/libraryfolders.vdf",
              QString(R"("libraryfolders" { "0" { "path" "%1" } })")
                  .arg(path("link-to-shared"))
                  .toUtf8());
        QVERIFY(QFile::link(shared(), path("link-to-shared")));
        QCOMPARE(names(OwnSteamGames::find(home(), shared(), {})),
                 QStringList({"Celeste", "Portal 2"}));
    }

    // Steam's whole folder inside the shared one is the shared folder too.
    void aLibraryInsideTheSharedFolder() {
        copyLibrary("other", shared() + "/more/steamapps");
        write(steamapps() + "/libraryfolders.vdf",
              QString(R"("libraryfolders" { "0" { "path" "%1" } })")
                  .arg(shared() + "/more")
                  .toUtf8());
        QCOMPARE(names(OwnSteamGames::find(home(), shared(), {})),
                 QStringList({"Celeste", "Portal 2"}));
    }

    void anOldListOfLibraries() {
        write(steamapps() + "/libraryfolders.vdf",
              QString(R"("LibraryFolders" { "TimeNextStatsReport" "1761700000"
                         "ContentStatsID" "-123" "1" "%1" })")
                  .arg(path("drive"))
                  .toUtf8());
        QCOMPARE(names(OwnSteamGames::find(home(), shared(), {})),
                 QStringList({"Celeste", "Half-Life 2", "Portal 2"}));
    }

    void aLibraryInTheHomeByTilde() {
        copyLibrary("other", home() + "/Games/steamapps");
        write(steamapps() + "/libraryfolders.vdf",
              R"("libraryfolders" { "1" { "path" "~/Games" } })");
        QCOMPARE(names(OwnSteamGames::find(home(), shared(), {})),
                 QStringList({"Celeste", "Half-Life 2", "Portal 2"}));
    }

    void librariesThatAreNotThere() {
        write(steamapps() + "/libraryfolders.vdf",
              QString(R"("libraryfolders" { "1" { "path" "%1" } "2" { "path" "drive" }
                         "3" { "path" "" } "4" { "label" "no path" } "5" { "path" { } } })")
                  .arg(path("gone"))
                  .toUtf8());
        QCOMPARE(OwnSteamGames::libraries(home() + "/.local/share/Steam", home()),
                 QStringList({QDir(steamapps()).canonicalPath()}));
    }

    // A list of libraries that cannot be read still leaves Steam's own.
    void aMalformedListOfLibraries() {
        // The path's string never closes.
        write(steamapps() + "/libraryfolders.vdf",
              QByteArray(R"("libraryfolders" { "1" { "path" )") + '"' + "/x } }");
        QCOMPARE(names(OwnSteamGames::find(home(), shared(), {})),
                 QStringList({"Celeste", "Portal 2"}));
    }

    void noSteam() {
        QVERIFY(QDir(home()).removeRecursively());
        QVERIFY(QDir().mkpath(home()));
        QVERIFY(OwnSteamGames::find(home(), shared(), {}).isEmpty());
        QVERIFY(OwnSteamGames::find(path("no-home"), shared(), {}).isEmpty());
    }

    // A Steam with no shared folder made yet still has its games named.
    void noSharedFolderYet() {
        QCOMPARE(names(OwnSteamGames::find(home(), path("not-made"), {})),
                 QStringList({"Celeste", "Portal 2"}));
    }
};

} // namespace

QTEST_GUILESS_MAIN(OwnSteamGamesTest)
#include "tst_ownsteamgames.moc"
