// SPDX-License-Identifier: Apache-2.0
#include "WaitingSave.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
#include <unistd.h>

namespace {

Account adaAccount() {
    return {.uid = 1002, .groups = {"ada", "cairn-l1"}, .login = "ada", .fullName = "Ada"};
}

mode_t modeOf(const QString& path) {
    struct stat status{};
    return lstat(QFile::encodeName(path).constData(), &status) == 0 ? status.st_mode & 07777 : 0;
}

class WaitingSaveTest : public QObject {
    Q_OBJECT

    QString path(const QString& name) const {
        return QDir(m_temporary.path()).filePath(name);
    }

    WaitingSave waiting(unsigned int owner = getuid()) const {
        return {m_folder, owner, path("restorecon")};
    }

    void give(const QString& login, const QByteArray& text) const {
        QVERIFY(QDir().mkpath(m_folder));
        QFile file(QDir(m_folder).filePath(login));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(text), text.size());
    }

    QStringList calls() const {
        QFile file(path("calls"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    }

    QTemporaryDir m_temporary;
    QString m_folder;

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        m_folder = path("games-waiting");
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/system-tool"), path("restorecon")));
    }

    void cleanup() {
        QDir(m_temporary.path()).removeRecursively();
        QDir().mkpath(m_temporary.path());
    }

    // One line, exactly the Save's own argument, which every account may
    // read; it reads back as written, and is labelled for its folder.
    void roundTrip() {
        const mode_t old = umask(077);
        const bool written = waiting().write("ada", {"570", "620"});
        umask(old);
        QVERIFY(written);
        QFile file(QDir(m_folder).filePath("ada"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("ada=570,620\n"));
        QCOMPARE(modeOf(m_folder), mode_t{0755});
        QCOMPARE(modeOf(file.fileName()), mode_t{0644});
        const WaitingSave::Read read = waiting().read(adaAccount());
        QCOMPARE(read.found, WaitingSave::Found::Waiting);
        QCOMPARE(read.appIds, QStringList({"570", "620"}));
        QCOMPARE(calls(), QStringList({"restorecon " + m_folder, "restorecon " + file.fileName()}));
    }

    void noFileIsNothing() {
        QCOMPARE(waiting().read(adaAccount()).found, WaitingSave::Found::Nothing);
        QVERIFY(QDir().mkpath(m_folder));
        QCOMPARE(waiting().read(adaAccount()).found, WaitingSave::Found::Nothing);
    }

    void removes() {
        QVERIFY(waiting().remove("ada"));
        QVERIFY(waiting().write("ada", {"620"}));
        QVERIFY(waiting().remove("ada"));
        QVERIFY(!QFileInfo::exists(QDir(m_folder).filePath("ada")));
        QVERIFY(waiting().remove("ada"));
    }

    void refusesWhatIsNotAList_data() {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("another child's") << QByteArray("ben=620\n");
        QTest::newRow("no games") << QByteArray("ada=\n");
        QTest::newRow("a bad appid") << QByteArray("ada=620,--all\n");
        QTest::newRow("a leading zero") << QByteArray("ada=0620\n");
        QTest::newRow("an appid twice") << QByteArray("ada=620,620\n");
        QTest::newRow("no end of line") << QByteArray("ada=620");
        QTest::newRow("two lines") << QByteArray("ada=620\nada=570\n");
        QTest::newRow("two children") << QByteArray("ada=620 ben=570\n");
        QTest::newRow("empty") << QByteArray();
        QByteArray tooMany = "ada=";
        for (int appId = 1; appId <= 1001; ++appId) {
            tooMany += QByteArray::number(appId) + (appId < 1001 ? "," : "\n");
        }
        QTest::newRow("more games than a Save takes") << tooMany;
        QTest::newRow("too big") << QByteArray("ada=620") + QByteArray(20000, ' ') + "\n";
    }

    // Anything else in the file is refused, never read as a list.
    void refusesWhatIsNotAList() {
        QFETCH(const QByteArray, text);
        give("ada", text);
        const WaitingSave::Read read = waiting().read(adaAccount());
        QCOMPARE(read.found, WaitingSave::Found::Refused);
        QVERIFY(read.appIds.isEmpty());
    }

    // Only a child Cairn made: an administrator, or an account in no level
    // group, never gets games from a waiting file.
    void refusesAnAccountThatIsNotAChild() {
        give("ada", "ada=620\n");
        Account admin = adaAccount();
        admin.groups.append("wheel");
        QCOMPARE(waiting().read(admin).found, WaitingSave::Found::Refused);
        Account plain = adaAccount();
        plain.groups = {"ada"};
        QCOMPARE(waiting().read(plain).found, WaitingSave::Found::Refused);
    }

    void refusesALink() {
        QVERIFY(QDir().mkpath(m_folder));
        QFile other(path("elsewhere"));
        QVERIFY(other.open(QIODevice::WriteOnly));
        other.write("ada=620\n");
        other.close();
        QVERIFY(QFile::link(path("elsewhere"), QDir(m_folder).filePath("ada")));
        QCOMPARE(waiting().read(adaAccount()).found, WaitingSave::Found::Refused);
    }

    void refusesAFolderThatIsALink() {
        QVERIFY(QDir().mkpath(path("elsewhere")));
        QVERIFY(QFile::link(path("elsewhere"), m_folder));
        QFile file(path("elsewhere/ada"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("ada=620\n");
        file.close();
        QCOMPARE(waiting().read(adaAccount()).found, WaitingSave::Found::Refused);
        QVERIFY(!waiting().write("ada", {"570"}));
        QVERIFY(!waiting().remove("ada"));
        QVERIFY(QFileInfo::exists(path("elsewhere/ada")));
    }

    // A file that is not the owner's, in a folder that is not, or in a
    // folder others may write in.
    void refusesTheWrongOwner() {
        give("ada", "ada=620\n");
        QCOMPARE(waiting(getuid() + 1).read(adaAccount()).found, WaitingSave::Found::Refused);
        QVERIFY(!waiting(getuid() + 1).write("ada", {"570"}));
    }

    void refusesAFolderOthersMayWrite() {
        give("ada", "ada=620\n");
        QVERIFY(chmod(QFile::encodeName(m_folder).constData(), 0777) == 0);
        QCOMPARE(waiting().read(adaAccount()).found, WaitingSave::Found::Refused);
        QVERIFY(!waiting().write("ada", {"570"}));
        QVERIFY(!waiting().remove("ada"));
    }

    void refusesWhatIsNotAFile() {
        QVERIFY(QDir().mkpath(QDir(m_folder).filePath("ada")));
        QCOMPARE(waiting().read(adaAccount()).found, WaitingSave::Found::Refused);
    }

    // A write that cannot be put in place leaves the old file as it was.
    void aFailedWriteLeavesTheOldFile() {
        QVERIFY(waiting().write("ada", {"620"}));
        QVERIFY(chmod(QFile::encodeName(m_folder).constData(), 0555) == 0);
        const bool written = waiting().write("ada", {"570"});
        QVERIFY(chmod(QFile::encodeName(m_folder).constData(), 0755) == 0);
        QVERIFY(!written);
        QCOMPARE(waiting().read(adaAccount()).appIds, QStringList({"620"}));
    }

    // The words a logout settles keep the child waiting; every other
    // ending, a clean one included, ends the wait.
    void keepsOnlyForWhatALogoutSettles() {
        for (const QString& word :
             QStringList{"logged-in", "still-running", "stopped", "not-started", "stuck"}) {
            QVERIFY2(WaitingSave::keeps(word), qPrintable(word));
        }
        for (const QString& word :
             QStringList{"busy", "copying", "failed", "not-set-up", "not-written", "no-library",
                         "no-reflink", "lists-shared"}) {
            QVERIFY2(!WaitingSave::keeps(word), qPrintable(word));
        }
        QVERIFY(!WaitingSave::keeps(std::nullopt));
    }
};

} // namespace

QTEST_GUILESS_MAIN(WaitingSaveTest)
#include "tst_waitingsave.moc"
