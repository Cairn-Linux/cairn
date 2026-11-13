// SPDX-License-Identifier: Apache-2.0
#include "AboutPage.h"

#include <QDir>
#include <QFile>
#include <QList>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

namespace {

using Place = AboutPage::Place;

// The file as the image build writes it (build_files/name-and-logos.sh).
constexpr const char* kImagesFile = FIXTURES "/os-release/cairn";

class AboutPageTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    static void write(const QString& path, const QByteArray& bytes) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    // A folder in the test's own, holding a file of each of these names.
    QString folderWith(const QString& name, const QStringList& texts) const {
        QDir().mkpath(path(name));
        for (const QString& text : texts) {
            write(QDir(path(name)).filePath(text), "words\n");
        }
        return path(name);
    }

    // A page's two places, both the test's own: never this computer's
    // os-release or licence folder. Nothing is given to open with.
    AboutPage::Setup setupOver(const QString& osRelease, const QString& licenceFolder = {}) const {
        AboutPage::Setup setup;
        setup.osRelease = osRelease;
        setup.licenceFolder = licenceFolder.isEmpty() ? path("no-licences") : licenceFolder;
        return setup;
    }

    const QStringList m_fullTexts = {"LICENSE", "LICENSE-CC-BY-SA-4.0", "NOTICE", "TRADEMARKS.md"};
    QTemporaryDir m_temporary;

private slots:
    void whatTheFileSays_data() {
        QTest::addColumn<QByteArray>("file");
        QTest::addColumn<QString>("version");
        QTest::addColumn<QString>("fedoraRelease");

        QFile fixture(kImagesFile);
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        // A later line for a key wins, so each row adds its lines to the
        // image's own.
        const QByteArray image = fixture.readAll();

        // No image writes its own version yet, and Fedora's number is
        // never shown as Cairn's.
        QTest::newRow("the image as it is built") << image << QString() << QString("45");
        QTest::newRow("with its version") << image + "IMAGE_VERSION=dev.20261112-0a1b2c3\n"
                                          << QString("dev.20261112-0a1b2c3") << QString("45");
        QTest::newRow("a quoted version") << image + "IMAGE_VERSION=\"1.0_rc-2\"\n"
                                          << QString("1.0_rc-2") << QString("45");
        QTest::newRow("64 characters") << image + "IMAGE_VERSION=" + QByteArray(64, 'a') + "\n"
                                       << QString(64, QChar('a')) << QString("45");
        QTest::newRow("65 characters") << image + "IMAGE_VERSION=" + QByteArray(65, 'a') + "\n"
                                       << QString() << QString("45");
        QTest::newRow("a space") << image + "IMAGE_VERSION=\"dev 1\"\n"
                                 << QString() << QString("45");
        QTest::newRow("a slash") << image + "IMAGE_VERSION=dev/1\n" << QString() << QString("45");
        QTest::newRow("markup") << image + "IMAGE_VERSION='<b>1</b>'\n"
                                << QString() << QString("45");
        QTest::newRow("a letter outside a to z")
            << image + QByteArray("IMAGE_VERSION=dév.1\n") << QString() << QString("45");
        QTest::newRow("an empty version") << image + "IMAGE_VERSION=\n"
                                          << QString() << QString("45");
        QTest::newRow("four digits") << image + "VERSION_ID=1234\n" << QString() << QString("1234");
        QTest::newRow("five digits") << image + "VERSION_ID=12345\n" << QString() << QString();
        QTest::newRow("a point release") << image + "VERSION_ID=45.1\n" << QString() << QString();
        QTest::newRow("rawhide") << image + "VERSION_ID=rawhide\n" << QString() << QString();
        QTest::newRow("no number") << image + "VERSION_ID=\n" << QString() << QString();
        // A developer's own computer: its file says nothing about Cairn.
        QTest::newRow("another system's file") << QByteArray("NAME=\"Fedora Linux\"\n"
                                                             "ID=fedora\n"
                                                             "VERSION_ID=45\n"
                                                             "IMAGE_VERSION=45.20261104\n")
                                               << QString() << QString();
        QTest::newRow("a file that names no system")
            << QByteArray("VERSION_ID=45\nIMAGE_VERSION=1.0\n") << QString() << QString();
        QTest::newRow("a name that only starts as Cairn's")
            << image + "ID=cairn-like\nIMAGE_VERSION=1.0\n"
            << QString() << QString();
        QTest::newRow("a name that differs only in a capital letter")
            << image + "ID=Cairn\nIMAGE_VERSION=1.0\n"
            << QString() << QString();
    }

    void whatTheFileSays() {
        QFETCH(const QByteArray, file);
        QFETCH(const QString, version);
        QFETCH(const QString, fedoraRelease);

        write(path("os-release"), file);
        const AboutPage page(setupOver(path("os-release")));
        QCOMPARE(page.version(), version);
        QCOMPARE(page.fedoraRelease(), fedoraRelease);
    }

    void withoutTheFile() {
        const AboutPage page(setupOver(path("no-os-release")));
        QCOMPARE(page.version(), QString());
        QCOMPARE(page.fedoraRelease(), QString());
        // What does not come from the file is still there.
        QCOMPARE(page.fedoraAddress(), QString("https://fedoraproject.org/"));
    }

    void theFullTexts() {
        const QString all = folderWith("all", m_fullTexts);
        const AboutPage page(setupOver(kImagesFile, all));
        QCOMPARE(page.licenceFolder(), all);
        QCOMPARE(page.fedoraAddress(), QString("https://fedoraproject.org/"));

        // One text missing, each in turn.
        for (const QString& missing : m_fullTexts) {
            QStringList others = m_fullTexts;
            others.removeOne(missing);
            const QString folder = folderWith("without-" + missing, others);
            QCOMPARE(AboutPage(setupOver(kImagesFile, folder)).licenceFolder(), QString());
        }
        // A text that is a folder.
        const QString folder =
            folderWith("a-folder", {"LICENSE", "LICENSE-CC-BY-SA-4.0", "NOTICE"});
        QVERIFY(QDir(folder).mkpath("TRADEMARKS.md"));
        QCOMPARE(AboutPage(setupOver(kImagesFile, folder)).licenceFolder(), QString());
        // No folder at all.
        QCOMPARE(AboutPage(setupOver(kImagesFile, path("nowhere"))).licenceFolder(), QString());
    }

    // Nothing phones home: an address is opened only by a press, and only
    // one of the page's own two.
    void opensOnlyWhatIsAsked() {
        const QString folder = folderWith("texts", m_fullTexts);
        QList<QUrl> opened;
        AboutPage::Setup setup = setupOver(kImagesFile, folder);
        setup.open = [&opened](const QUrl& address) {
            opened.append(address);
            return true;
        };
        AboutPage page(setup);
        // Making the page and reading all of it opens nothing.
        QCOMPARE(page.version(), QString());
        QCOMPARE(page.fedoraRelease(), QString("45"));
        QCOMPARE(page.licenceFolder(), folder);
        QCOMPARE(page.fedoraAddress(), QString("https://fedoraproject.org/"));
        QCOMPARE(page.notOpened(), Place::None);
        QCOMPARE(opened, QList<QUrl>());

        page.open(Place::None);
        QCOMPARE(opened, QList<QUrl>());

        page.open(Place::FedoraWebsite);
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.constLast().toString(), QString("https://fedoraproject.org/"));

        page.open(Place::LicenceFolder);
        QCOMPARE(opened.size(), 2);
        QVERIFY(opened.constLast().isLocalFile());
        QCOMPARE(opened.constLast().toLocalFile(), folder);
        QCOMPARE(page.notOpened(), Place::None);

        // Without the full texts there is no folder to open.
        setup.licenceFolder = path("nowhere");
        AboutPage without(setup);
        without.open(Place::LicenceFolder);
        QCOMPARE(opened.size(), 2);
        QCOMPARE(without.notOpened(), Place::None);
        without.open(Place::FedoraWebsite);
        QCOMPARE(opened.size(), 3);
        QCOMPARE(opened.constLast().toString(), QString("https://fedoraproject.org/"));
    }

    void nothingOpened() {
        const QString folder = folderWith("to-open", m_fullTexts);
        bool opens = true;
        AboutPage::Setup setup = setupOver(kImagesFile, folder);
        setup.open = [&opens](const QUrl&) { return opens; };
        AboutPage page(setup);
        const QSignalSpy changed(&page, &AboutPage::notOpenedChanged);
        QCOMPARE(page.notOpened(), Place::None);

        page.open(Place::FedoraWebsite);
        page.open(Place::LicenceFolder);
        QCOMPARE(page.notOpened(), Place::None);
        QCOMPARE(changed.count(), 0);

        opens = false;
        page.open(Place::FedoraWebsite);
        QCOMPARE(page.notOpened(), Place::FedoraWebsite);
        QCOMPARE(changed.count(), 1);
        // The same press again changes nothing, so the signal is sent once.
        page.open(Place::FedoraWebsite);
        QCOMPARE(page.notOpened(), Place::FedoraWebsite);
        QCOMPARE(changed.count(), 1);
        page.open(Place::LicenceFolder);
        QCOMPARE(page.notOpened(), Place::LicenceFolder);
        QCOMPARE(changed.count(), 2);

        // A later press that works clears it.
        opens = true;
        page.open(Place::LicenceFolder);
        QCOMPARE(page.notOpened(), Place::None);
        QCOMPARE(changed.count(), 3);

        // With nothing to open with, as when a test gives none.
        AboutPage bare(setupOver(kImagesFile, folder));
        const QSignalSpy bareChanged(&bare, &AboutPage::notOpenedChanged);
        bare.open(Place::LicenceFolder);
        QCOMPARE(bare.notOpened(), Place::LicenceFolder);
        QCOMPARE(bareChanged.count(), 1);
        bare.open(Place::FedoraWebsite);
        QCOMPARE(bare.notOpened(), Place::FedoraWebsite);
        QCOMPARE(bareChanged.count(), 2);
        bare.open(Place::None);
        QCOMPARE(bare.notOpened(), Place::FedoraWebsite);
        QCOMPARE(bareChanged.count(), 2);
    }
};

} // namespace

QTEST_GUILESS_MAIN(AboutPageTest)
#include "tst_aboutpage.moc"
