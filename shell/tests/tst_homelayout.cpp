// SPDX-License-Identifier: Apache-2.0
#include "HomeLayout.h"

#include <QTemporaryFile>
#include <QTest>

namespace {

QString writeTemporary(QTemporaryFile& file, const QByteArray& json) {
    if (!file.open()) {
        return {};
    }
    file.write(json);
    file.close();
    return file.fileName();
}

class HomeLayoutTest : public QObject {
    Q_OBJECT

private slots:
    void builtInHasTheStarterSet() {
        const QList<World::Node> nodes = HomeLayout::builtIn(QStringLiteral("Sam"));
        QCOMPARE(nodes.size(), 7);
        QCOMPARE(nodes[0].path, QStringLiteral("home/sam"));
        QCOMPARE(nodes[1].path, QStringLiteral("home/sam/hello"));
        QCOMPARE(nodes[1].kind, World::Kind::Note);
        QVERIFY(nodes[1].text.contains(QStringLiteral("Sam")));
        QCOMPARE(nodes[6].path, QStringLiteral("home/sam/pictures"));
        QCOMPARE(nodes[6].kind, World::Kind::Folder);
    }

    void aNamelessChildIsMe() {
        QCOMPARE(HomeLayout::builtIn(QString())[0].path, QStringLiteral("home/me"));
    }

    void readsAVersionOneFile() {
        QTemporaryFile file;
        const QString path = writeTemporary(file, R"({"version": 1, "entries": [
            {"path": "notes"},
            {"path": "notes/from-mum", "text": "Have a good day."}
        ]})");
        const HomeLayout::Result result = HomeLayout::read(path, QStringLiteral("Sam"));
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.nodes.size(), 3);
        QCOMPARE(result.nodes[1].path, QStringLiteral("home/sam/notes"));
        QCOMPARE(result.nodes[1].kind, World::Kind::Folder);
        QCOMPARE(result.nodes[2].name, QStringLiteral("from-mum"));
        QCOMPARE(result.nodes[2].text, QStringLiteral("Have a good day."));
    }

    void refusesWithASentence_data() {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("expected");
        QTest::newRow("bad json") << QByteArray("{nope") << "is not valid JSON.";
        QTest::newRow("wrong version")
            << QByteArray(R"({"version": 2, "entries": []})") << "is not version 1.";
        QTest::newRow("absolute path")
            << QByteArray(R"({"version": 1, "entries": [{"path": "/etc"}]})")
            << "has an entry with a bad path.";
        QTest::newRow("dot dot") << QByteArray(R"({"version": 1, "entries": [{"path": "../x"}]})")
                                 << "has an entry with a bad path.";
    }

    void refusesWithASentence() {
        QFETCH(const QByteArray, json);
        QFETCH(const QString, expected);
        QTemporaryFile file;
        const QString path = writeTemporary(file, json);
        const HomeLayout::Result result = HomeLayout::read(path, QStringLiteral("Sam"));
        QVERIFY(result.nodes.isEmpty());
        QVERIFY2(result.error.endsWith(expected), qPrintable(result.error));
    }

    void missingFileIsASentence() {
        const HomeLayout::Result result =
            HomeLayout::read(QStringLiteral("/nonexistent/home.json"), QStringLiteral("Sam"));
        QVERIFY(result.error.endsWith(QStringLiteral("could not be read.")));
    }
};

} // namespace

QTEST_GUILESS_MAIN(HomeLayoutTest)
#include "tst_homelayout.moc"
