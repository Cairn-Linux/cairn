// SPDX-License-Identifier: Apache-2.0
#include "PictureFile.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

namespace {

// An image red on its left half and blue on its right.
QString halves(const QTemporaryDir& folder, int width, int height, const char* format) {
    QImage image(width, height, QImage::Format_RGB32);
    QPainter painter(&image);
    painter.fillRect(0, 0, width / 2, height, Qt::red);
    painter.fillRect(width / 2, 0, width - (width / 2), height, Qt::blue);
    painter.end();
    const QString path =
        QDir(folder.path()).filePath(QStringLiteral("source.") + QString::fromLatin1(format));
    return image.save(path, format) ? path : QString();
}

class PictureFileTest : public QObject {
    Q_OBJECT

private slots:
    // A wide photo becomes a square from its middle, no wider than the
    // side, as a PNG.
    void aWidePictureBecomesASquareFromItsMiddle() {
        const QTemporaryDir folder;
        const QString source = halves(folder, 1200, 600, "jpg");
        QVERIFY(!source.isEmpty());
        const QString target = folder.filePath(QStringLiteral("picture.png"));
        QVERIFY(PictureFile::prepare(source, target));
        QFile file(target);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.read(8).startsWith("\x89PNG"));
        const QImage made(target);
        QCOMPARE(made.size(), QSize(PictureFile::side, PictureFile::side));
        // The middle half of the width: its left half red, its right blue.
        QVERIFY(QColor(made.pixel(PictureFile::side / 8, PictureFile::side / 2)).red() > 200);
        QVERIFY(QColor(made.pixel(PictureFile::side * 7 / 8, PictureFile::side / 2)).blue() > 200);
        // Far below AccountsService's limit of a megabyte.
        QVERIFY(QFileInfo(target).size() < qint64{1024} * 1024);
    }

    void aTallPictureToo() {
        const QTemporaryDir folder;
        const QString source = halves(folder, 300, 900, "png");
        const QString target = folder.filePath(QStringLiteral("picture.png"));
        QVERIFY(PictureFile::prepare(source, target));
        QCOMPARE(QImage(target).size(), QSize(PictureFile::side, PictureFile::side));
    }

    // A picture smaller than the side is not made bigger.
    void aSmallPictureKeepsItsSize() {
        const QTemporaryDir folder;
        const QString source = halves(folder, 100, 60, "png");
        const QString target = folder.filePath(QStringLiteral("picture.png"));
        QVERIFY(PictureFile::prepare(source, target));
        QCOMPARE(QImage(target).size(), QSize(60, 60));
    }

    void notAPicture() {
        const QTemporaryDir folder;
        const QString source = folder.filePath(QStringLiteral("notes.png"));
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not a picture at all");
        file.close();
        QVERIFY(!PictureFile::prepare(source, folder.filePath(QStringLiteral("picture.png"))));
        QVERIFY(!PictureFile::prepare(folder.filePath(QStringLiteral("missing.png")),
                                      folder.filePath(QStringLiteral("picture.png"))));
    }

    void aTargetThatCannotBeWritten() {
        const QTemporaryDir folder;
        const QString source = halves(folder, 100, 100, "png");
        QVERIFY(
            !PictureFile::prepare(source, folder.filePath(QStringLiteral("no/such/picture.png"))));
    }
};

} // namespace

QTEST_MAIN(PictureFileTest)
#include "tst_picturefile.moc"
