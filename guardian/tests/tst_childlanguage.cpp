// SPDX-License-Identifier: Apache-2.0
#include "ChildLanguage.h"
#include "LanguageFile.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
#include <unistd.h>

namespace {

mode_t modeOf(const QString& path) {
    struct stat status{};
    return lstat(QFile::encodeName(path).constData(), &status) == 0 ? status.st_mode & 07777 : 0;
}

QByteArray contentsOf(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Every [Formats] key plasma-workspace 6.8's startplasma reads.
QStringList formatKeys() {
    return {"LANG",        "LC_ADDRESS",   "LC_COLLATE",     "LC_CTYPE", "LC_IDENTIFICATION",
            "LC_MONETARY", "LC_MESSAGES",  "LC_MEASUREMENT", "LC_NAME",  "LC_NUMERIC",
            "LC_PAPER",    "LC_TELEPHONE", "LC_TIME",        "LC_ALL"};
}

class ChildLanguageTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_temporary;

    QString parent() const {
        return QDir(m_temporary.path()).filePath("locale");
    }

    QString fileOf(const QString& login) const {
        return QDir(parent()).filePath(login + "/plasma-localerc");
    }

private slots:
    void init() {
        QDir(parent()).removeRecursively();
    }

    // The maintainer's short list, US English first (#299).
    void theTable() {
        QCOMPARE(ChildLanguage::all().size(), std::size_t{2});
        QCOMPARE(QString::fromLatin1(ChildLanguage::all()[0].locale),
                 QStringLiteral("en_US.UTF-8"));
        QCOMPARE(QString::fromLatin1(ChildLanguage::all()[1].locale),
                 QStringLiteral("en_GB.UTF-8"));
        QCOMPARE(ChildLanguage::nameOf(ChildLanguage::all()[0]), QStringLiteral("English (US)"));
        QCOMPARE(ChildLanguage::nameOf(ChildLanguage::all()[1]), QStringLiteral("English (UK)"));
    }

    void onlyALocaleInTheTable_data() {
        QTest::addColumn<QString>("locale");
        QTest::addColumn<bool>("found");
        QTest::newRow("US") << "en_US.UTF-8" << true;
        QTest::newRow("UK") << "en_GB.UTF-8" << true;
        QTest::newRow("installed but not offered") << "de_DE.UTF-8" << false;
        QTest::newRow("no encoding") << "en_GB" << false;
        QTest::newRow("small letters") << "en_gb.utf-8" << false;
        QTest::newRow("a line after") << "en_GB.UTF-8\nLC_ALL=C" << false;
        QTest::newRow("empty") << "" << false;
    }

    void onlyALocaleInTheTable() {
        QFETCH(const QString, locale);
        QFETCH(const bool, found);
        QCOMPARE(ChildLanguage::find(locale) != nullptr, found);
    }

    // Every key is locked, so neither the child's own plasma-localerc nor
    // their environment.d can fill one in (ADR-0058).
    void theFileLocksEveryKey() {
        const QByteArray text = ChildLanguage::fileText(*ChildLanguage::find("en_GB.UTF-8"));
        const QList<QByteArray> lines = text.split('\n');
        for (const QString& key : formatKeys()) {
            QVERIFY2(lines.contains((key + "[$i]=en_GB.UTF-8").toUtf8()), qPrintable(key));
        }
        QVERIFY(lines.contains("LANGUAGE[$i]=en_GB:en"));
        QVERIFY(text.indexOf("[Formats]") < text.indexOf("LANG[$i]"));
        QVERIFY(text.indexOf("[Translations]") < text.indexOf("LANGUAGE[$i]"));
        QVERIFY(text.indexOf("[Translations]") > text.indexOf("LC_ALL[$i]"));
        QCOMPARE(ChildLanguage::localeIn(text), std::optional<QString>("en_GB.UTF-8"));
    }

    void aFileFamilyDidNotWriteGivesNoLocale() {
        QCOMPARE(ChildLanguage::localeIn("[Formats]\nLANG=en_GB.UTF-8\n"), std::nullopt);
        QCOMPARE(ChildLanguage::localeIn("[Formats]\nLANG[$i]=de_DE.UTF-8\n"), std::nullopt);
        QCOMPARE(ChildLanguage::localeIn(""), std::nullopt);
    }

    // Both folders and the file are made, readable by every account and
    // writable by their owner alone.
    void writeMakesTheFolders() {
        const auto* uk = ChildLanguage::find("en_GB.UTF-8");
        QVERIFY(LanguageFile::write(parent(), getuid(), "/nowhere/restorecon", "ada", *uk));
        QCOMPARE(modeOf(parent()), mode_t{0755});
        QCOMPARE(modeOf(QDir(parent()).filePath("ada")), mode_t{0755});
        QCOMPARE(modeOf(fileOf("ada")), mode_t{0644});
        QCOMPARE(contentsOf(fileOf("ada")), ChildLanguage::fileText(*uk));
        QCOMPARE(LanguageFile::read(parent(), "ada"), std::optional<QString>("en_GB.UTF-8"));
        QCOMPARE(LanguageFile::read(parent(), "ben"), std::nullopt);
        const auto* us = ChildLanguage::find("en_US.UTF-8");
        QVERIFY(LanguageFile::write(parent(), getuid(), "/nowhere/restorecon", "ada", *us));
        QCOMPARE(LanguageFile::read(parent(), "ada"), std::optional<QString>("en_US.UTF-8"));
    }

    // A link in place of a child's folder is never written through.
    void aLinkIsNeverFollowed() {
        const QTemporaryDir elsewhere;
        QVERIFY(QDir().mkpath(parent()));
        QVERIFY(QFile::link(elsewhere.path(), QDir(parent()).filePath("ada")));
        QVERIFY(!LanguageFile::write(parent(), getuid(), "/nowhere/restorecon", "ada",
                                     *ChildLanguage::find("en_GB.UTF-8")));
        QVERIFY(!QFile::exists(QDir(elsewhere.path()).filePath("plasma-localerc")));
    }

    void aFolderOthersMayWriteIsRefused() {
        QVERIFY(QDir().mkpath(parent()));
        QVERIFY(
            QFile::setPermissions(parent(), QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                                QFileDevice::ExeOwner | QFileDevice::WriteOther |
                                                QFileDevice::ExeOther | QFileDevice::ReadOther));
        QVERIFY(!LanguageFile::write(parent(), getuid(), "/nowhere/restorecon", "ada",
                                     *ChildLanguage::find("en_GB.UTF-8")));
        QVERIFY(!LanguageFile::remove(parent(), getuid(), "ada"));
    }

    void removeTakesTheFile() {
        QVERIFY(LanguageFile::remove(parent(), getuid(), "ada"));
        QVERIFY(LanguageFile::write(parent(), getuid(), "/nowhere/restorecon", "ada",
                                    *ChildLanguage::find("en_GB.UTF-8")));
        QVERIFY(LanguageFile::remove(parent(), getuid(), "ada"));
        QVERIFY(!QFile::exists(fileOf("ada")));
        QCOMPARE(LanguageFile::read(parent(), "ada"), std::nullopt);
        QVERIFY(LanguageFile::remove(parent(), getuid(), "ada"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(ChildLanguageTest)
#include "tst_childlanguage.moc"
