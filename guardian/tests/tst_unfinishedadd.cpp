// SPDX-License-Identifier: Apache-2.0
#include "UnfinishedAdd.h"

#include <QTest>

namespace {

// No account or group has any name: every login is free.
bool nothingInUse(const QString& /*name*/) {
    return false;
}

bool everythingInUse(const QString& /*name*/) {
    return true;
}

class UnfinishedAddTest : public QObject {
    Q_OBJECT

private slots:
    void nothingIsRememberedAtFirst() {
        const UnfinishedAdd add;
        QCOMPARE(add.loginFor(QStringLiteral("Cy"), {}, nothingInUse), std::nullopt);
    }

    // A failure, or an answer that could not be read, may have made the
    // account; a refusal or a closed box did not.
    void whatIsRemembered_data() {
        QTest::addColumn<QString>("answer");
        QTest::addColumn<bool>("readable");
        QTest::addColumn<bool>("remembered");
        QTest::newRow("failed") << "failed" << true << true;
        QTest::newRow("unreadable") << "unknown" << false << true;
        QTest::newRow("done") << "done" << true << false;
        QTest::newRow("busy") << "busy" << true << false;
        QTest::newRow("dismissed") << "dismissed" << true << false;
    }

    void whatIsRemembered() {
        QFETCH(const QString, answer);
        QFETCH(const bool, readable);
        QFETCH(const bool, remembered);
        UnfinishedAdd add;
        add.starting(QStringLiteral("cy"), QStringLiteral("Cy"));
        add.answered(answer, readable);
        QCOMPARE(add.loginFor(QStringLiteral(" cy "), {}, nothingInUse).has_value(), remembered);
    }

    // The account the add made is still that child, or the login is free.
    void onlyWhileTheLoginIsStillTheirs() {
        UnfinishedAdd add;
        add.starting(QStringLiteral("cy"), QStringLiteral("Cy"));
        add.answered(QStringLiteral("failed"), true);
        const std::vector<ListedChild> cy{{.login = "cy", .name = "Cy", .level = 1}};
        QCOMPARE(add.loginFor(QStringLiteral("Cy"), cy, everythingInUse),
                 std::optional<QString>(QStringLiteral("cy")));
        const std::vector<ListedChild> other{{.login = "cy", .name = "Cy B", .level = 1}};
        QCOMPARE(add.loginFor(QStringLiteral("Cy"), other, everythingInUse), std::nullopt);
        QCOMPARE(add.loginFor(QStringLiteral("Dee"), {}, nothingInUse), std::nullopt);
    }

    // Done with that login forgets it.
    void aFinishedAddIsForgotten() {
        UnfinishedAdd add;
        add.starting(QStringLiteral("cy"), QStringLiteral("Cy"));
        add.answered(QStringLiteral("failed"), true);
        add.starting(QStringLiteral("cy"), QStringLiteral("Cy"));
        add.answered(QStringLiteral("done"), true);
        QCOMPARE(add.loginFor(QStringLiteral("Cy"), {}, nothingInUse), std::nullopt);
    }
};

} // namespace

QTEST_GUILESS_MAIN(UnfinishedAddTest)
#include "tst_unfinishedadd.moc"
