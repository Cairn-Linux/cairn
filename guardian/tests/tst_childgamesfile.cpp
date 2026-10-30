// SPDX-License-Identifier: Apache-2.0
#include "ChildGamesFile.h"
#include "GameCatalogue.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>
#include <unistd.h>

namespace {

QJsonObject game(const QString& appId, const QString& title) {
    return {{"id", "steam-" + appId},
            {"title", title},
            {"category", "games"},
            {"engine", "steam"},
            {"exec", QJsonArray{"steam", "-applaunch", appId}}};
}

mode_t modeOf(const QString& path) {
    struct stat status{};
    return lstat(QFile::encodeName(path).constData(), &status) == 0 ? status.st_mode & 07777 : 0;
}

class ChildGamesFileTest : public QObject {
    Q_OBJECT

    QStringList calls() const {
        QFile file(QDir(m_temporary.path()).filePath("calls"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
    }

    QTemporaryDir m_temporary;
    QString m_folder;
    QString m_restorecon;

private slots:
    void init() {
        QVERIFY(m_temporary.isValid());
        m_folder = QDir(m_temporary.path()).filePath("games");
        m_restorecon = QDir(m_temporary.path()).filePath("restorecon");
        QVERIFY(QFile::link(QStringLiteral(FIXTURES "/system-tool"), m_restorecon));
    }

    void cleanup() {
        QDir(m_temporary.path()).removeRecursively();
        QDir().mkpath(m_temporary.path());
    }

    // Whatever the umask, the folder is 0755 and the file 0644, so every
    // child's launcher reads it; it reads back as written.
    void writesAFileEveryoneReads() {
        const mode_t old = umask(077);
        const ChildGamesFile file(m_folder, getuid(), m_restorecon);
        const QList<QJsonObject> entries{game("620", "Portal 2"), game("12", "Zoo Tycoon")};
        const bool written = file.write("ada", entries);
        umask(old);
        QVERIFY(written);
        QCOMPARE(file.path("ada"), QDir(m_folder).filePath("ada.json"));
        QCOMPARE(modeOf(m_folder), mode_t{0755});
        QCOMPARE(modeOf(file.path("ada")), mode_t{0644});
        QCOMPARE(file.read("ada"), entries);
        QFile text(file.path("ada"));
        QVERIFY(text.open(QIODevice::ReadOnly));
        QVERIFY(text.readAll().contains("\"version\": 1"));
    }

    // A new file is labelled again; the same list again is left alone.
    void labelsOnlyAChange() {
        const ChildGamesFile file(m_folder, getuid(), m_restorecon);
        QVERIFY(file.write("ada", {game("620", "Portal 2")}));
        QVERIFY(file.write("ada", {game("620", "Portal 2")}));
        QCOMPARE(calls(),
                 QStringList({"restorecon " + m_folder, "restorecon " + file.path("ada")}));
        QVERIFY(file.write("ada", {}));
        QCOMPARE(calls().size(), 3);
        QVERIFY(file.read("ada").isEmpty());
    }

    // A list already in place is written, even when labelling it fails.
    void aFailedLabelIsStillWritten() {
        const ChildGamesFile file(m_folder, getuid(), m_restorecon);
        QVERIFY(file.write("ada", {game("620", "Portal 2")}));
        QFile fail(QDir(m_temporary.path()).filePath("fail-restorecon"));
        QVERIFY(fail.open(QIODevice::WriteOnly));
        fail.close();
        QVERIFY(file.write("ada", {game("12", "Zoo Tycoon")}));
        QCOMPARE(file.read("ada"), QList<QJsonObject>({game("12", "Zoo Tycoon")}));
        QCOMPARE(calls().size(), 3);
    }

    // A folder that is not root's own plain folder is never written in.
    void refusesAFolderItCannotTrust() {
        QVERIFY(QDir().mkpath(QDir(m_temporary.path()).filePath("elsewhere")));
        QVERIFY(QFile::link(QDir(m_temporary.path()).filePath("elsewhere"), m_folder));
        QVERIFY(!ChildGamesFile(m_folder, getuid(), m_restorecon).write("ada", {}));
        QVERIFY(!QFileInfo::exists(QDir(m_temporary.path()).filePath("elsewhere/ada.json")));
        QVERIFY(QFile::remove(m_folder));

        QVERIFY(QDir().mkpath(m_folder));
        QVERIFY(!ChildGamesFile(m_folder, getuid() + 1, m_restorecon).write("ada", {}));
        QVERIFY(chmod(QFile::encodeName(m_folder).constData(), 0775) == 0);
        QVERIFY(!ChildGamesFile(m_folder, getuid(), m_restorecon).write("ada", {}));
        QVERIFY(!QFileInfo::exists(QDir(m_folder).filePath("ada.json")));
        QVERIFY(QDir(m_folder).removeRecursively());

        QFile plain(m_folder);
        QVERIFY(plain.open(QIODevice::WriteOnly));
        plain.close();
        QVERIFY(!ChildGamesFile(m_folder, getuid(), m_restorecon).write("ada", {}));
    }

    // A list anyone but root may write is not a list at all, and a list of
    // the right bytes left writable by an older build is repaired, not
    // trusted. A link in the file's place is never read through and never
    // written through.
    void refusesWritableAndLinkedFilesAndRepairsMode() {
        const ChildGamesFile file(m_folder, getuid(), m_restorecon);
        const QList<QJsonObject> portal{game("620", "Portal 2")};
        QVERIFY(file.write("ada", portal));
        for (mode_t mode : {mode_t{0664}, mode_t{0666}, mode_t{0646}}) {
            QVERIFY(chmod(QFile::encodeName(file.path("ada")).constData(), mode) == 0);
            QVERIFY(file.read("ada").isEmpty());
        }
        QVERIFY(chmod(QFile::encodeName(file.path("ada")).constData(), 0644) == 0);
        QCOMPARE(file.read("ada"), portal);

        // The same list with an unsafe mode is repaired, not left writable.
        QVERIFY(chmod(QFile::encodeName(file.path("ada")).constData(), 0666) == 0);
        QVERIFY(file.write("ada", portal));
        QCOMPARE(modeOf(file.path("ada")), mode_t{0644});
        QCOMPARE(file.read("ada"), portal);

        // A link in the file's place is replaced, not followed: the write
        // lands on the file and leaves what the link pointed at alone.
        const QString elsewhere = QDir(m_temporary.path()).filePath("elsewhere.json");
        QVERIFY(QFile::copy(file.path("ada"), elsewhere));
        QVERIFY(QFile::remove(file.path("ada")));
        QVERIFY(QFile::link(elsewhere, file.path("ada")));
        QVERIFY(file.read("ada").isEmpty());
        const QList<QJsonObject> zoo{game("12", "Zoo Tycoon")};
        QVERIFY(file.write("ada", zoo));
        QVERIFY(!QFileInfo(file.path("ada")).isSymLink());
        QCOMPARE(modeOf(file.path("ada")), mode_t{0644});
        QCOMPARE(file.read("ada"), zoo);
        QFile pointedAt(elsewhere);
        QVERIFY(pointedAt.open(QIODevice::ReadOnly));
        QCOMPARE(pointedAt.readAll(), GameCatalogue::toJson(portal));
    }

    // A list that cannot be read is no games: missing, broken, another
    // version, or too big.
    void readsABadFileAsEmpty() {
        const ChildGamesFile file(m_folder, getuid(), m_restorecon);
        QVERIFY(file.read("ada").isEmpty());
        QVERIFY(QDir().mkpath(m_folder));
        for (const QByteArray& text :
             {QByteArray("{not json"), QByteArray(R"({"version": 2, "entries": []})"),
              QByteArray(R"({"version": 1})"), QByteArray("[]"),
              QByteArray(R"({"version": 1, "entries": [)") +
                  QByteArray(qsizetype{5} * 1024 * 1024, ' ') + QByteArray(R"(]})")}) {
            QFile broken(file.path("ada"));
            QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
            broken.write(text);
            broken.close();
            QVERIFY(file.read("ada").isEmpty());
        }
    }
};

} // namespace

QTEST_GUILESS_MAIN(ChildGamesFileTest)
#include "tst_childgamesfile.moc"
