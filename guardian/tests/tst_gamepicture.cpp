// SPDX-License-Identifier: Apache-2.0
#include "GamePicture.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

namespace {

class GamePictureTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_temporary;
    QString m_picture;

private slots:
    void initTestCase() {
        QVERIFY(m_temporary.isValid());
        m_picture = QDir(m_temporary.path()).filePath("1001.jpg");
        QFile file(m_picture);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("not really a picture") > 0);
    }

    void aFileHereIsShown() {
        const QJsonObject entry{{"picture", m_picture}};
        QCOMPARE(GamePicture::fromEntry(entry), QUrl::fromLocalFile(m_picture));
    }

    void noPictureKeyIsNoPicture() {
        QVERIFY(GamePicture::fromEntry({}).isEmpty());
    }

    // The launcher's rule: only the full path of a file on this computer.
    void anythingElseIsNoPicture_data() {
        QTest::addColumn<QJsonValue>("picture");
        QTest::newRow("not text") << QJsonValue(7);
        QTest::newRow("a list") << QJsonValue(QJsonArray{m_picture});
        QTest::newRow("empty") << QJsonValue(QString());
        QTest::newRow("relative") << QJsonValue(QStringLiteral("1001.jpg"));
        QTest::newRow("a web address") << QJsonValue(QStringLiteral("http://127.0.0.1:9/x.png"));
        QTest::newRow("a file URL") << QJsonValue(QUrl::fromLocalFile(m_picture).toString());
        QTest::newRow("the program's own") << QJsonValue(QStringLiteral(":/qt/qml/x.png"));
        QTest::newRow("missing") << QJsonValue(m_temporary.path() + "/1002.jpg");
        QTest::newRow("a folder") << QJsonValue(m_temporary.path());
    }

    void anythingElseIsNoPicture() {
        QFETCH(const QJsonValue, picture);
        const QJsonObject entry{{"picture", picture}};
        QVERIFY(GamePicture::fromEntry(entry).isEmpty());
    }
};

} // namespace

QTEST_GUILESS_MAIN(GamePictureTest)
#include "tst_gamepicture.moc"
