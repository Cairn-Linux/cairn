// SPDX-License-Identifier: Apache-2.0
#include "GamePicture.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

namespace {

// A game's picture by its app id, with the launcher's rule (ADR-0047).
class GamePictureTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    void write(const QString& name) const {
        QFile file(path(name));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("not really a picture") > 0);
    }

    QTemporaryDir m_temporary;

private slots:
    void initTestCase() {
        QVERIFY(m_temporary.isValid());
        QVERIFY(QDir().mkpath(path("pictures/1001")));
        write("pictures/1001.jpg");
    }

    void aFileThereIsShown() {
        QCOMPARE(GamePicture::forAppId(path("pictures"), "1001"),
                 QUrl::fromLocalFile(path("pictures/1001.jpg")));
    }

    void noFileIsNoPicture() {
        QVERIFY(GamePicture::forAppId(path("pictures"), "1002").isEmpty());
    }

    // Only a plain file in a folder named by its full path, on this
    // computer. The call is made from the temporary folder, and the test
    // program holds a picture at :/pictures/1001.jpg (CMakeLists.txt), so
    // the relative folder and the program's own each have a file there
    // for the rule to refuse.
    void noFolderOrAnOddOneIsNoPicture_data() {
        QTest::addColumn<QString>("folder");
        QTest::addColumn<QString>("reachable");
        QTest::newRow("none") << QString() << QString();
        QTest::newRow("relative") << QStringLiteral("pictures")
                                  << QStringLiteral("pictures/1001.jpg");
        QTest::newRow("the program's own")
            << QStringLiteral(":/pictures") << QStringLiteral(":/pictures/1001.jpg");
        QTest::newRow("missing") << path("no-such-folder") << QString();
    }

    void noFolderOrAnOddOneIsNoPicture() {
        QFETCH(const QString, folder);
        QFETCH(const QString, reachable);
        const QString started = QDir::currentPath();
        QVERIFY(QDir::setCurrent(m_temporary.path()));
        const bool there = reachable.isEmpty() || QFileInfo(reachable).isFile();
        const bool none = GamePicture::forAppId(folder, "1001").isEmpty();
        QVERIFY(QDir::setCurrent(started));
        QVERIFY2(there, qPrintable(reachable));
        QVERIFY(none);
    }

    void aFolderOrALinkIsNoPicture() {
        QVERIFY(QDir().mkpath(path("odd/1001.jpg")));
        QVERIFY(GamePicture::forAppId(path("odd"), "1001").isEmpty());
        QVERIFY(QFile::link(path("pictures/1001.jpg"), path("odd/1002.jpg")));
        QVERIFY(GamePicture::forAppId(path("odd"), "1002").isEmpty());
    }

    // The path is made from the app id, so anything else gives no picture,
    // even when a file is there for it to reach.
    void anythingButAnAppIdIsNoPicture_data() {
        QTest::addColumn<QString>("appId");
        QTest::addColumn<QString>("reachable");
        QTest::newRow("up a folder") << QStringLiteral("../1001") << QStringLiteral("1001.jpg");
        QTest::newRow("through a folder")
            << QStringLiteral("1001/../1001") << QStringLiteral("pictures/1001.jpg");
        QTest::newRow("a leading zero")
            << QStringLiteral("01") << QStringLiteral("pictures/01.jpg");
        QTest::newRow("a file name")
            << QStringLiteral("1001.jpg") << QStringLiteral("pictures/1001.jpg.jpg");
        QTest::newRow("nothing") << QString() << QStringLiteral("pictures/.jpg");
    }

    void anythingButAnAppIdIsNoPicture() {
        QFETCH(const QString, appId);
        QFETCH(const QString, reachable);
        write(reachable);
        QVERIFY(GamePicture::forAppId(path("pictures"), appId).isEmpty());
    }
};

} // namespace

QTEST_GUILESS_MAIN(GamePictureTest)
#include "tst_gamepicture.moc"
