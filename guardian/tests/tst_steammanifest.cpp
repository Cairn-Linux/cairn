// SPDX-License-Identifier: Apache-2.0
#include "SteamManifest.h"

#include <QFile>
#include <QRegularExpression>
#include <QTest>

namespace {

KeyValues manifest(const QString& body) {
    const std::optional<KeyValues> parsed = KeyValues::parse(u"\"AppState\" {" + body + u"}");
    return parsed.value_or(KeyValues());
}

// A game as "appid name", or "nothing".
QString described(const std::optional<SteamManifest::Game>& game) {
    return game ? game->appId + u' ' + game->name : QStringLiteral("nothing");
}

class SteamManifestTest : public QObject {
    Q_OBJECT

private slots:
    void aGame() {
        QCOMPARE(described(SteamManifest::gameIn(
                     "appmanifest_620.acf",
                     manifest(R"("appid" "620" "name" "Portal 2" "installdir" "Portal 2")"))),
                 "620 Portal 2");
    }

    void aGameWithNoNameIsNamedForItsFolder() {
        QCOMPARE(described(SteamManifest::gameIn(
                     "appmanifest_620.acf", manifest(R"("appid" "620" "installdir" "Portal 2")"))),
                 "620 Portal 2");
        QCOMPARE(
            described(SteamManifest::gameIn(
                "appmanifest_620.acf", manifest(R"("appid" "620" "name" "" "installdir" "P2")"))),
            "620 P2");
    }

    // Where Steam is putting a game is what matters, so one still
    // downloading, or on a beta branch, is named too.
    void aGameStillDownloadingIsAGame() {
        QVERIFY(SteamManifest::gameIn(
            "appmanifest_620.acf",
            manifest(R"("appid" "620" "name" "Portal 2" "installdir" "Portal 2"
                        "StateFlags" "1026" "UserConfig" { "BetaKey" "beta" })")));
    }

    void toolsAreNotGames_data() {
        QTest::addColumn<QString>("appId");
        QTest::addColumn<QString>("name");
        QTest::newRow("redistributables") << "228980" << "Steamworks Common Redistributables";
        QTest::newRow("runtime 1.0") << "1070560" << "Steam Linux Runtime 1.0 (scout)";
        QTest::newRow("soldier") << "1391110" << "Steam Linux Runtime 2.0 (soldier)";
        QTest::newRow("sniper") << "1628350" << "Steam Linux Runtime 3.0 (sniper)";
        QTest::newRow("Proton Experimental") << "1493710" << "Proton Experimental";
        QTest::newRow("a Proton by name") << "2805730" << "Proton 9.0";
        QTest::newRow("a runtime by name") << "3810310" << "steam linux runtime 4.0";
        QTest::newRow("Steamworks by name") << "1" << "STEAMWORKS SDK Redist";
        QTest::newRow("a listed appid, renamed") << "1070560" << "Something else";
    }

    void toolsAreNotGames() {
        QFETCH(const QString, appId);
        QFETCH(const QString, name);
        QVERIFY(SteamManifest::isTool(appId, name));
        QCOMPARE(
            SteamManifest::gameIn(
                "appmanifest_" + appId + ".acf",
                manifest(QString(R"("appid" "%1" "name" "%2" "installdir" "x")").arg(appId, name)))
                .has_value(),
            false);
    }

    // A name that only has a tool's word later on is a game.
    void aGameNamedLikeATool() {
        QVERIFY(!SteamManifest::isTool("620", "Portal with Proton jokes"));
    }

    void manifestsKidscanWouldNotRead_data() {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QString>("body");
        QTest::newRow("no appid") << "appmanifest_620.acf" << R"("name" "P" "installdir" "P")";
        QTest::newRow("appid not a number")
            << "appmanifest_62x.acf" << R"("appid" "62x" "name" "P" "installdir" "P")";
        QTest::newRow("appid from zero")
            << "appmanifest_0620.acf" << R"("appid" "0620" "name" "P" "installdir" "P")";
        QTest::newRow("appid too long") << "appmanifest_12345678901.acf"
                                        << R"("appid" "12345678901" "name" "P" "installdir" "P")";
        QTest::newRow("appid with a new line")
            << "appmanifest_620\n.acf" << "\"appid\" \"620\n\" \"name\" \"P\" \"installdir\" \"P\"";
        QTest::newRow("another file's appid")
            << "appmanifest_440.acf" << R"("appid" "620" "name" "P" "installdir" "P")";
        QTest::newRow("no folder") << "appmanifest_620.acf" << R"("appid" "620" "name" "P")";
        QTest::newRow("a folder path")
            << "appmanifest_620.acf" << R"("appid" "620" "name" "P" "installdir" "a/b")";
        QTest::newRow("folder up") << "appmanifest_620.acf"
                                   << R"("appid" "620" "name" "P" "installdir" "..")";
        QTest::newRow("a copy's own folder")
            << "appmanifest_620.acf" << R"("appid" "620" "name" "P" "installdir" ".cairn-x")";
        QTest::newRow("a name that is a block")
            << "appmanifest_620.acf" << R"("appid" "620" "name" { } "installdir" "P")";
        QTest::newRow("an appid that is a block")
            << "appmanifest_620.acf" << R"("appid" { } "name" "P" "installdir" "P")";
    }

    void manifestsKidscanWouldNotRead() {
        QFETCH(const QString, fileName);
        QFETCH(const QString, body);
        QCOMPARE(SteamManifest::gameIn(fileName, manifest(body)).has_value(), false);
    }

    void appStateMustBeABlock() {
        const std::optional<KeyValues> parsed = KeyValues::parse(u"\"AppState\" \"620\"");
        QVERIFY(parsed);
        QCOMPARE(
            SteamManifest::gameIn("appmanifest_620.acf", parsed.value_or(KeyValues())).has_value(),
            false);
    }

    // The rule is kidscan's: its list of tools and its name pattern are the
    // ones here, so the two cannot drift apart unnoticed.
    void theRuleIsKidscans() {
        QFile file(KIDSCAN);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString source = QString::fromUtf8(file.readAll());
        const QRegularExpression block("APPID_DENYLIST = \\{([^}]*)\\}");
        const QRegularExpressionMatch found = block.match(source);
        QVERIFY(found.hasMatch());
        QStringList listed;
        const QRegularExpression quoted(R"re("([0-9]+)")re");
        for (const QRegularExpressionMatch& appId : quoted.globalMatch(found.captured(1))) {
            listed.append(appId.captured(1));
        }
        QStringList ours = SteamManifest::toolAppIds();
        listed.sort();
        ours.sort();
        QCOMPARE(listed, ours);
        QVERIFY(source.contains(
            R"re(PROTON_NAME_RE = re.compile(r"^(proton|steam linux runtime|steamworks)", re.I))re"));
        QVERIFY(source.contains(R"(APPID = re.compile(r"[1-9][0-9]{0,9}"))"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(SteamManifestTest)
#include "tst_steammanifest.moc"
