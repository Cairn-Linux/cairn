// SPDX-License-Identifier: Apache-2.0
#include "DoorsFromTiles.h"
#include "OutputModel.h"
#include "TerminalSession.h"
#include "TileModel.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {

// What the shell answered, without the lines the child typed.
QStringList answers(const OutputModel* output) {
    QStringList out;
    for (const OutputModel::Line& line : output->lines()) {
        if (!line.isInput) {
            out.append(line.text);
        }
    }
    return out;
}

class DoorsFromTilesTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
    }

    // A games tile is a door in Footpath's games folder, opened with the same
    // command as the tile; the Terminal tile is never a door (ADR-0024, #97).
    void gamesTilesBecomeTheGamesFolder() {
        QFile manifest(m_dir.filePath(QStringLiteral("games.json")));
        QVERIFY(manifest.open(QIODevice::WriteOnly));
        manifest.write(R"({"version": 1, "entries": [
            {"title": "Draw", "category": "make", "exec": ["tuxpaint"]},
            {"title": "Putt-Putt", "category": "games", "exec": ["steam", "-applaunch", "283920"]}]})");
        manifest.close();

        TileModel tiles(this);
        tiles.setManifestPath(manifest.fileName());
        TerminalSession session(this);
        DoorsFromTiles doors(this);
        doors.setTiles(&tiles);
        doors.setSession(&session);

        session.run(QStringLiteral("ls"));
        QCOMPARE(
            answers(session.output()),
            (QStringList{QStringLiteral("make"), QStringLiteral("games"), QStringLiteral("home")}));
        session.run(QStringLiteral("cd games"));
        session.run(QStringLiteral("ls"));
        QCOMPARE(answers(session.output()).last(), QStringLiteral("putt-putt"));

        const QSignalSpy launches(&session, &TerminalSession::launchRequested);
        session.run(QStringLiteral("open putt-putt"));
        QCOMPARE(launches.size(), 1);
        QCOMPARE(launches.at(0).at(0).toString(), QStringLiteral("Putt-Putt"));
        QCOMPARE(launches.at(0).at(1).toStringList(),
                 (QStringList{QStringLiteral("steam"), QStringLiteral("-applaunch"),
                              QStringLiteral("283920")}));
    }
};

} // namespace

QTEST_GUILESS_MAIN(DoorsFromTilesTest)
#include "tst_doorsfromtiles.moc"
