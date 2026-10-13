// SPDX-License-Identifier: Apache-2.0
#include "FailedLogin.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {

using Reason = FailedLogin::Reason;

// A list of children on a visit, as the root helper writes it.
QString writeList(const QTemporaryDir& dir, const QByteArray& text) {
    const QString path = dir.filePath(QStringLiteral("steam-visits"));
    QFile list(path);
    if (list.open(QIODevice::WriteOnly)) {
        list.write(text);
    }
    return path;
}

class FailedLoginTest : public QObject {
    Q_OBJECT

private slots:
    void theDefaultIsTheListPamReads() {
        const FailedLogin failed;
        QCOMPARE(failed.visitsFile().toLocalFile(), QStringLiteral("/run/cairn/steam-visits"));
    }

    void aChildOnAVisitIsTheReason() {
        const QTemporaryDir dir;
        FailedLogin failed;
        failed.setVisitsFile(QUrl::fromLocalFile(writeList(dir, "ada\n")));
        QCOMPARE(failed.reasonFor(QStringLiteral("ada"), false), Reason::SteamVisit);
    }

    // PAM refuses a listed child whatever the password, so a right one
    // is not called wrong.
    void theVisitComesBeforeThePassword() {
        const QTemporaryDir dir;
        FailedLogin failed;
        failed.setVisitsFile(QUrl::fromLocalFile(writeList(dir, "ben\n")));
        QCOMPARE(failed.reasonFor(QStringLiteral("ben"), true), Reason::SteamVisit);
    }

    void anotherChildIsNotOnAVisit() {
        const QTemporaryDir dir;
        FailedLogin failed;
        failed.setVisitsFile(QUrl::fromLocalFile(writeList(dir, "ada\n")));
        QCOMPARE(failed.reasonFor(QStringLiteral("ben"), false), Reason::NeedsGrownUp);
        QCOMPARE(failed.reasonFor(QStringLiteral("guardian"), true), Reason::WrongPassword);
    }

    // Only a whole line names an account.
    void aNameInsideAnotherIsNotIt() {
        const QTemporaryDir dir;
        const QString list = writeList(dir, "adam\n");
        QVERIFY(!FailedLogin::lists(list, QStringLiteral("ada")));
        QVERIFY(FailedLogin::lists(list, QStringLiteral("adam")));
    }

    void everyLineCounts() {
        const QTemporaryDir dir;
        const QString list = writeList(dir, "ada\nben");
        QVERIFY(FailedLogin::lists(list, QStringLiteral("ada")));
        QVERIFY(FailedLogin::lists(list, QStringLiteral("ben")));
    }

    void noListNamesNoOne() {
        QVERIFY(!FailedLogin::lists(QStringLiteral("/nonexistent/steam-visits"),
                                    QStringLiteral("ada")));
        const FailedLogin failed;
        QCOMPARE(failed.reasonFor(QStringLiteral("nobody-here"), false), Reason::NeedsGrownUp);
    }

    void anEmptyNameIsNeverListed() {
        const QTemporaryDir dir;
        QVERIFY(!FailedLogin::lists(writeList(dir, "\n"), QString()));
    }
};

} // namespace

QTEST_GUILESS_MAIN(FailedLoginTest)
#include "tst_failedlogin.moc"
