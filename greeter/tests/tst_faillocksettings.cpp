// SPDX-License-Identifier: Apache-2.0
#include "FaillockSettings.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

namespace {

// A settings file with this text.
QString writeSettings(const QTemporaryDir& dir, const QByteArray& text) {
    const QString path = dir.filePath(QStringLiteral("faillock.conf"));
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(text);
    }
    return path;
}

std::optional<FaillockSettings> readText(const QByteArray& text) {
    const QTemporaryDir dir;
    return FaillockSettings::read(writeSettings(dir, text));
}

class FaillockSettingsTest : public QObject {
    Q_OBJECT

private slots:
    // faillock's own defaults when there is no file (pam_faillock.c).
    void noFileGivesFaillocksDefaults() {
        const auto settings = FaillockSettings::read(QStringLiteral("/nonexistent/faillock.conf"));
        QVERIFY(settings.has_value());
        QCOMPARE(settings->dir, QStringLiteral("/var/run/faillock"));
        QCOMPARE(settings->deny, 3);
        QCOMPARE(settings->failInterval, 900);
        QCOMPARE(settings->unlockTime, 600);
    }

    void anEmptyFileGivesTheDefaults() {
        const auto settings = readText("");
        QVERIFY(settings.has_value());
        QCOMPARE(settings->deny, 3);
    }

    // The file the image and provisioning install.
    void cairnsOwnFile() {
        const auto settings = FaillockSettings::read(QStringLiteral(CAIRN_FAILLOCK_CONF));
        QVERIFY(settings.has_value());
        QCOMPARE(settings->dir, QStringLiteral("/var/lib/faillock"));
        QCOMPARE(settings->deny, 3);
        QCOMPARE(settings->failInterval, 900);
        QCOMPARE(settings->unlockTime, 600);
    }

    void everySettingTheRuleUses() {
        const auto settings =
            readText("dir = /tmp/f\ndeny = 5\nfail_interval = 60\nunlock_time = 120\n");
        QVERIFY(settings.has_value());
        QCOMPARE(settings->dir, QStringLiteral("/tmp/f"));
        QCOMPARE(settings->deny, 5);
        QCOMPARE(settings->failInterval, 60);
        QCOMPARE(settings->unlockTime, 120);
    }

    // faillock's parser takes a space or an '=' after the key, comments
    // after '#', and spaces anywhere around them.
    void theWaysFaillockSplitsALine_data() {
        QTest::addColumn<QByteArray>("line");
        QTest::newRow("spaces around =") << QByteArray("deny = 5\n");
        QTest::newRow("no spaces") << QByteArray("deny=5\n");
        QTest::newRow("a space and no =") << QByteArray("deny 5\n");
        QTest::newRow("a tab") << QByteArray("deny\t5\n");
        QTest::newRow("leading spaces") << QByteArray("   deny = 5   \n");
        QTest::newRow("a comment after") << QByteArray("deny = 5 # five\n");
        QTest::newRow("a space before =") << QByteArray("deny =5\n");
        QTest::newRow("no newline at the end") << QByteArray("deny = 5");
    }
    void theWaysFaillockSplitsALine() {
        QFETCH(const QByteArray, line);
        const auto settings = readText("# a comment\n\n" + line);
        QVERIFY(settings.has_value());
        QCOMPARE(settings->deny, 5);
    }

    void neverIsZero() {
        const auto settings = readText("unlock_time = never\n");
        QVERIFY(settings.has_value());
        QCOMPARE(settings->unlockTime, 0);
    }

    void zeroIsAllowed() {
        const auto settings = readText("deny = 0\nfail_interval = 0\n");
        QVERIFY(settings.has_value());
        QCOMPARE(settings->deny, 0);
        QCOMPARE(settings->failInterval, 0);
    }

    // Seven days is the most faillock takes; deny is an unsigned short.
    void theLargestValuesFaillockTakes() {
        const auto settings =
            readText("deny = 65535\nfail_interval = 604800\nunlock_time = 604800\n");
        QVERIFY(settings.has_value());
        QCOMPARE(settings->deny, 65535);
        QCOMPARE(settings->failInterval, 604800);
        QCOMPARE(settings->unlockTime, 604800);
    }

    // Settings that change what faillock logs or prints, never when it
    // locks.
    void harmlessFlagsAreTaken() {
        const auto settings = readText("audit\nsilent\nno_log_info\nlocal_users_only\nnodelay\n");
        QVERIFY(settings.has_value());
        QCOMPARE(settings->deny, 3);
    }

    // Anything faillock would refuse or that this copy does not follow
    // means "not sure", so the screen never claims a lock.
    void anythingUnclearIsNotUnderstood_data() {
        QTest::addColumn<QByteArray>("text");
        QTest::newRow("admin_group") << QByteArray("admin_group = wheel\n");
        QTest::newRow("even_deny_root") << QByteArray("even_deny_root\n");
        QTest::newRow("root_unlock_time") << QByteArray("root_unlock_time = 60\n");
        QTest::newRow("an unknown key") << QByteArray("lockout = 5\n");
        QTest::newRow("a relative dir") << QByteArray("dir = faillock\n");
        QTest::newRow("an empty dir") << QByteArray("dir =\n");
        QTest::newRow("deny with no number") << QByteArray("deny\n");
        QTest::newRow("deny below zero") << QByteArray("deny = -1\n");
        QTest::newRow("deny too big") << QByteArray("deny = 65536\n");
        QTest::newRow("deny with letters") << QByteArray("deny = 3x\n");
        QTest::newRow("a fraction") << QByteArray("unlock_time = 1.5\n");
        QTest::newRow("unlock_time past 7 days") << QByteArray("unlock_time = 604801\n");
        QTest::newRow("fail_interval past 7 days") << QByteArray("fail_interval = 604801\n");
        QTest::newRow("a huge number") << QByteArray("fail_interval = 99999999999999999999\n");
        QTest::newRow("never for deny") << QByteArray("deny = never\n");
        QTest::newRow("two =") << QByteArray("deny == 3\n");
    }
    void anythingUnclearIsNotUnderstood() {
        QFETCH(const QByteArray, text);
        QVERIFY(!readText(text).has_value());
    }

    void aFileThatCannotBeReadIsNotUnderstood() {
        if (geteuid() == 0) {
            QSKIP("root reads any file");
        }
        const QTemporaryDir dir;
        const QString path = writeSettings(dir, "deny = 3\n");
        QVERIFY(QFile::setPermissions(path, QFileDevice::Permissions()));
        QVERIFY(!FaillockSettings::read(path).has_value());
    }

    void aFolderIsNotASettingsFile() {
        const QTemporaryDir dir;
        QVERIFY(!FaillockSettings::read(dir.path()).has_value());
    }
};

} // namespace

QTEST_GUILESS_MAIN(FaillockSettingsTest)
#include "tst_faillocksettings.moc"
