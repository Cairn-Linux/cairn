// SPDX-License-Identifier: Apache-2.0
#include "SteamEntry.h"

#include <QProcess>
#include <QTest>

namespace {

class SteamEntryTest : public QObject {
    Q_OBJECT

private slots:
    void theWholeEntry() {
        QCOMPARE(SteamEntry::text("ada", "Ada", "/usr/bin/cairn-steam-for"),
                 QStringLiteral("[Desktop Entry]\n"
                                "Type=Application\n"
                                "Name=Steam for Ada\n"
                                "Comment=Open Ada's own Steam, to sign in or change its settings\n"
                                "Exec=/usr/bin/cairn-steam-for ada\n"
                                "TryExec=/usr/bin/cairn-steam-for\n"
                                "Icon=steam\n"
                                "Categories=Game;\n"));
    }

    void itsPlaceNamesTheChild() {
        QCOMPARE(SteamEntry::path("/usr/local/share/applications", "ben"),
                 QStringLiteral("/usr/local/share/applications/cairn-steam-for-ben.desktop"));
    }

    // Provisioning's steam_entry, run from the script itself, writes the very
    // same text, so a machine set up by hand gets the same entries.
    void provisioningWritesTheSame() {
        QProcess bash;
        bash.start(
            QStringLiteral("/bin/bash"),
            {QStringLiteral("-c"),
             QStringLiteral("source \"$1\" 2>/dev/null; set +x; "
                            "known_as() { echo 'Ben Jones'; }; PREFIX=/usr; steam_entry ben"),
             QStringLiteral("bash"), QStringLiteral(PROVISION_SCRIPT)});
        QVERIFY(bash.waitForFinished());
        QCOMPARE(bash.exitCode(), 0);
        QCOMPARE(QString::fromUtf8(bash.readAllStandardOutput()),
                 SteamEntry::text("ben", "Ben Jones", "/usr/bin/cairn-steam-for"));
    }
};

} // namespace

QTEST_APPLESS_MAIN(SteamEntryTest)
#include "tst_steamentry.moc"
