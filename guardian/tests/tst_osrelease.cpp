// SPDX-License-Identifier: Apache-2.0
#include "OsRelease.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

// The file as the image build writes it (build_files/name-and-logos.sh).
constexpr const char* kImagesFile = FIXTURES "/os-release/cairn";

// Every way a value may be written, with lines that are not values.
constexpr const char* kQuotesFile = FIXTURES "/os-release/quotes";

// What a file read into. Each test checks first that it read; one that
// did not holds nothing here.
OsRelease held(const std::optional<OsRelease>& read) {
    return read.value_or(OsRelease());
}

class OsReleaseTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    static void write(const QString& path, const QByteArray& bytes) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }

    QTemporaryDir m_temporary;

private slots:
    void theImagesFile() {
        const std::optional<OsRelease> read = OsRelease::read(kImagesFile);
        QVERIFY(read);
        const OsRelease release = held(read);
        QCOMPARE(release.value("NAME"), QString("Cairn Linux"));
        QCOMPARE(release.value("PRETTY_NAME"), QString("Cairn Linux"));
        QCOMPARE(release.value("ID"), QString("cairn"));
        QCOMPARE(release.value("ID_LIKE"), QString("fedora"));
        QCOMPARE(release.value("VERSION_ID"), QString("45"));
        QCOMPARE(release.value("VERSION"), QString("45"));
        QCOMPARE(release.value("ANSI_COLOR"), QString("0;38;2;165;200;214"));
        QCOMPARE(release.value("LOGO"), QString("cairn-logo-on-ink"));
        QCOMPARE(release.value("DEFAULT_HOSTNAME"), QString("cairn"));
        QCOMPARE(release.value("HOME_URL"), QString("https://cairnlinux.com/"));
        QCOMPARE(release.value("BUG_REPORT_URL"),
                 QString("https://github.com/Cairn-Linux/cairn/issues"));
        QCOMPARE(release.value("SUPPORT_END"), QString("2027-12-08"));
        // Without its single quotes.
        QCOMPARE(release.value("OSTREE_VERSION"), QString("45.20261104"));
        QCOMPARE(release.value("IMAGE_ID"), QString("cairn"));
        // No image writes Cairn's own version yet.
        QCOMPARE(release.value("IMAGE_VERSION").has_value(), false);
    }

    void quotesAndComments() {
        const std::optional<OsRelease> read = OsRelease::read(kQuotesFile);
        QVERIFY(read);
        const OsRelease release = held(read);
        QCOMPARE(release.value("BARE"), QString("plain-1.2"));
        QCOMPARE(release.value("SINGLE"), QString("two words, a \"quote\" and a \\ kept"));
        QCOMPARE(release.value("DOUBLE"), QString("two words"));
        QCOMPARE(release.value("ESCAPED"), QString("a \"quote\", a \\, a $ and a `"));
        QCOMPARE(release.value("OTHER"), QString("a \\n stays as it is written"));
        QCOMPARE(release.value("EMPTY"), QString(""));
        QCOMPARE(release.value("EMPTY_QUOTED"), QString(""));
        QCOMPARE(release.value("INDENTED"), QString("yes"));
        QCOMPARE(release.value("TWICE"), QString("second"));
        // Nothing is expanded or run.
        QCOMPARE(release.value("HOME_DIR"), QString("$HOME"));
        QCOMPARE(release.value("COMMAND"), QString("`id`"));
        QCOMPARE(release.value("SUBSHELL"), QString("$(id)"));
        QCOMPARE(release.value("WITH_EQUALS"), QString("a=b"));
        // Not assignments.
        QCOMPARE(release.value("").has_value(), false);
        QCOMPARE(release.value("a line with no equals sign").has_value(), false);
        // A comment is passed over, also when it looks like an assignment.
        QCOMPARE(release.value("#COMMENTED").has_value(), false);
        QCOMPARE(release.value("# INDENTED_COMMENT").has_value(), false);
        QCOMPARE(release.value("MISSING").has_value(), false);
    }

    void unreadable() {
        QCOMPARE(OsRelease::read(path("missing")).has_value(), false);
        // A folder.
        QVERIFY(QDir(m_temporary.path()).mkpath("folder"));
        QCOMPARE(OsRelease::read(path("folder")).has_value(), false);
        // Not a plain file, though it opens and reads as an empty one.
        QCOMPARE(OsRelease::read("/dev/null").has_value(), false);
        // Too big, and the largest that is read.
        write(path("big"), QByteArray("ID=cairn\n") + QByteArray(OsRelease::kFileLimit - 8, '#'));
        QCOMPARE(OsRelease::read(path("big")).has_value(), false);
        write(path("biggest"),
              QByteArray("ID=cairn\n") + QByteArray(OsRelease::kFileLimit - 9, '#'));
        QCOMPARE(held(OsRelease::read(path("biggest"))).value("ID"), QString("cairn"));
        // A link to a good file, as /etc/os-release is.
        QVERIFY(QFile::link(QString::fromUtf8(kImagesFile), path("link")));
        QCOMPARE(held(OsRelease::read(path("link"))).value("ID"), QString("cairn"));
        // A link to nothing.
        QVERIFY(QFile::link(path("missing"), path("broken")));
        QCOMPARE(OsRelease::read(path("broken")).has_value(), false);
        // An empty file reads, and holds nothing.
        write(path("empty"), "");
        QVERIFY(OsRelease::read(path("empty")));
        QCOMPARE(held(OsRelease::read(path("empty"))).value("ID").has_value(), false);
    }

    void oddValues() {
        write(path("odd"), QByteArray("ID=cairn\n"
                                      "BELL=\"ring\a\"\n"
                                      "ESCAPE=\x1b[31mred\n"
                                      "TAB='a\tb'\n"
                                      "UNCLOSED=\"never ends\n"
                                      "UNCLOSED_SINGLE='never ends\n"
                                      "AFTER=\"closed\"more\n"
                                      "AFTER_SINGLE='closed'more\n"
                                      "INNER_SINGLE='it's'\n"
                                      "LONG=") +
                               QByteArray(10000, 'x') +
                               QByteArray("\n"
                                          "LONGEST=") +
                               QByteArray(OsRelease::kLongestValue, 'x') +
                               QByteArray("\n"
                                          "TOO_LONG=") +
                               QByteArray(OsRelease::kLongestValue + 1, 'x') +
                               QByteArray("\n"
                                          "GOOD_THEN_BAD=fine\n"
                                          "GOOD_THEN_BAD=\"never ends\n"
                                          "VERSION_ID=45\r\n"
                                          "LAST=still read"));
        const std::optional<OsRelease> read = OsRelease::read(path("odd"));
        QVERIFY(read);
        const OsRelease release = held(read);
        QCOMPARE(release.value("BELL").has_value(), false);
        QCOMPARE(release.value("ESCAPE").has_value(), false);
        QCOMPARE(release.value("TAB").has_value(), false);
        QCOMPARE(release.value("UNCLOSED").has_value(), false);
        QCOMPARE(release.value("UNCLOSED_SINGLE").has_value(), false);
        QCOMPARE(release.value("AFTER").has_value(), false);
        QCOMPARE(release.value("AFTER_SINGLE").has_value(), false);
        QCOMPARE(release.value("INNER_SINGLE").has_value(), false);
        QCOMPARE(release.value("LONG").has_value(), false);
        QCOMPARE(release.value("LONGEST"), QString(OsRelease::kLongestValue, QChar('x')));
        QCOMPARE(release.value("TOO_LONG").has_value(), false);
        // The later line wins, also when it cannot be used.
        QCOMPARE(release.value("GOOD_THEN_BAD").has_value(), false);
        // The other keys of the same file still read.
        QCOMPARE(release.value("ID"), QString("cairn"));
        QCOMPARE(release.value("VERSION_ID"), QString("45"));
        QCOMPARE(release.value("LAST"), QString("still read"));
    }
};

} // namespace

QTEST_GUILESS_MAIN(OsReleaseTest)
#include "tst_osrelease.moc"
