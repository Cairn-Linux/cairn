// SPDX-License-Identifier: Apache-2.0
#include "KeyValues.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

namespace {

const char* const kManifest = R"(// A comment, as Steam never writes but Valve's format allows
"AppState"
{
	"appid"		"620"
	"name"		"Portal 2"
	"InstalledDepots"
	{
		"621"
		{
			"manifest"		"1234"
		}
	}
}
)";

// What a text parsed into. Each test checks first that it parsed; one that
// did not holds nothing here.
KeyValues held(const std::optional<KeyValues>& parsed) {
    return parsed.value_or(KeyValues());
}

class KeyValuesTest : public QObject {
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
    void aManifestIsRead() {
        const std::optional<KeyValues> parsed = KeyValues::parse(QString::fromUtf8(kManifest));
        QVERIFY(parsed);
        QCOMPARE(held(parsed).value({"AppState", "appid"}), QString("620"));
        QCOMPARE(held(parsed).value({"AppState", "name"}), QString("Portal 2"));
        QCOMPARE(held(parsed).value({"AppState", "InstalledDepots", "621", "manifest"}),
                 QString("1234"));
        QCOMPARE(held(parsed).keys({"AppState"}),
                 QStringList({"appid", "name", "InstalledDepots"}));
    }

    // Steam is not consistent about case.
    void keysMatchWithoutCase() {
        const std::optional<KeyValues> parsed = KeyValues::parse(QString::fromUtf8(kManifest));
        QVERIFY(parsed);
        QCOMPARE(held(parsed).value({"appstate", "APPID"}), QString("620"));
    }

    void aBlockIsNotAValue() {
        const std::optional<KeyValues> parsed = KeyValues::parse(QString::fromUtf8(kManifest));
        QVERIFY(parsed);
        QCOMPARE(held(parsed).value({"AppState", "InstalledDepots"}), std::nullopt);
        QCOMPARE(held(parsed).value({"AppState"}), std::nullopt);
        QCOMPARE(held(parsed).value({"AppState", "appid", "deeper"}), std::nullopt);
        QCOMPARE(held(parsed).value({"AppState", "missing"}), std::nullopt);
        QCOMPARE(held(parsed).keys({"AppState", "name"}), QStringList());
    }

    // As in kidscan, a key given again keeps its last value, so a name
    // given as a block after a string is not a name.
    void aKeyGivenTwiceKeepsTheLast() {
        std::optional<KeyValues> parsed =
            KeyValues::parse(QString(R"("a" { "name" "first" "NAME" "other" "name" "last" })"));
        QVERIFY(parsed);
        QCOMPARE(held(parsed).value({"a", "name"}), QString("last"));
        QCOMPARE(held(parsed).keys({"a"}), QStringList({"name", "NAME"}));
        parsed = KeyValues::parse(QString(R"("a" { "name" "first" "name" { } })"));
        QVERIFY(parsed);
        QCOMPARE(held(parsed).value({"a", "name"}), std::nullopt);
    }

    void aBackslashKeepsTheNextCharacter() {
        const std::optional<KeyValues> parsed =
            KeyValues::parse(QString(R"("name" "Say \"hello\" \\ there")"));
        QVERIFY(parsed);
        QCOMPARE(held(parsed).value({"name"}), QString(R"(Say "hello" \ there)"));
    }

    // Unquoted words are passed over, as kidscan's tokens pass over them.
    void wordsOutsideQuotesArePassedOver() {
        const std::optional<KeyValues> parsed = KeyValues::parse(u"junk \"a\" / \"b\" #x");
        QVERIFY(parsed);
        QCOMPARE(held(parsed).value({"a"}), QString("b"));
    }

    void anEmptyTextHasNothing() {
        const std::optional<KeyValues> parsed = KeyValues::parse(u"");
        QVERIFY(parsed);
        QCOMPARE(held(parsed).keys({}), QStringList());
    }

    void malformedTextIsUnreadable_data() {
        QTest::addColumn<QString>("text");
        QTest::newRow("a string that never closes")
            << QString(R"("AppState" { "name" )") + u'"' + "Portal";
        QTest::newRow("a block that never closes") << R"("AppState" { "name" "Portal 2")";
        QTest::newRow("a brace too many") << "\"AppState\" { } }";
        QTest::newRow("a block with no key") << R"({ "name" "Portal 2" })";
        QTest::newRow("a block after a block") << R"("a" { } { })";
        QTest::newRow("deeper than Steam goes")
            << QString("\"a\" {").repeated(40) + QString("}").repeated(40);
        QTest::newRow("a file of opening braces") << QString("\"a\" {").repeated(200000);
    }

    void malformedTextIsUnreadable() {
        QFETCH(const QString, text);
        QCOMPARE(KeyValues::parse(text).has_value(), false);
    }

    void asDeepAsAllowed() {
        const std::optional<KeyValues> parsed = KeyValues::parse(
            QString("\"a\" {").repeated(32) + R"("b" "c")" + QString("}").repeated(32));
        QVERIFY(parsed);
    }

    void aFileIsRead() {
        write(path("appmanifest_620.acf"), kManifest);
        const std::optional<KeyValues> parsed = KeyValues::readFile(path("appmanifest_620.acf"));
        QVERIFY(parsed);
        QCOMPARE(held(parsed).value({"AppState", "name"}), QString("Portal 2"));
    }

    void aFileAsBigAsAllowed() {
        QByteArray bytes = R"("name" "Portal 2")";
        bytes.append(QByteArray(KeyValues::kFileLimit - bytes.size(), ' '));
        write(path("biggest.acf"), bytes);
        QVERIFY(KeyValues::readFile(path("biggest.acf")));
    }

    void unreadableFiles() {
        QCOMPARE(KeyValues::readFile(path("missing.acf")).has_value(), false);
        // A folder.
        QVERIFY(QDir(m_temporary.path()).mkpath("folder.acf"));
        QCOMPARE(KeyValues::readFile(path("folder.acf")).has_value(), false);
        // Too big.
        write(path("big.acf"), QByteArray(KeyValues::kFileLimit + 1, ' '));
        QCOMPARE(KeyValues::readFile(path("big.acf")).has_value(), false);
        // A link, even to a good file.
        write(path("good.acf"), kManifest);
        QVERIFY(QFile::link(path("good.acf"), path("link.acf")));
        QCOMPARE(KeyValues::readFile(path("link.acf")).has_value(), false);
        // A named pipe with no writer, which would otherwise wait for ever.
        QCOMPARE(::mkfifo(QFile::encodeName(path("pipe.acf")).constData(), 0600), 0);
        QCOMPARE(KeyValues::readFile(path("pipe.acf")).has_value(), false);
        // Malformed.
        // The name's string never closes.
        write(path("bad.acf"), QByteArray(R"("AppState" { "name" )") + '"' + "Portal");
        QCOMPARE(KeyValues::readFile(path("bad.acf")).has_value(), false);
    }
};

} // namespace

QTEST_GUILESS_MAIN(KeyValuesTest)
#include "tst_keyvalues.moc"
