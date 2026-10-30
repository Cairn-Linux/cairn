// SPDX-License-Identifier: Apache-2.0
#include "NewPassword.h"

#include <QTest>

namespace {

using Problem = NewPassword::Problem;

class NewPasswordTest : public QObject {
    Q_OBJECT

private slots:
    void check_data() {
        QTest::addColumn<QString>("password");
        QTest::addColumn<std::optional<Problem>>("problem");

        QTest::newRow("a plain one") << "CairnOS" << std::optional<Problem>();
        QTest::newRow("one character") << "x" << std::optional<Problem>();
        // A colon is fine: chpasswd splits its line at the first one,
        // after the login.
        QTest::newRow("colons, spaces and symbols")
            << " a:b c$HOME*;'\" " << std::optional<Problem>();
        QTest::newRow("accents") << QStringLiteral("Zoë Ångström") << std::optional<Problem>();
        QTest::newRow("an emoji counts as one")
            << QString(NewPassword::maxLength - 1, QLatin1Char('a')) + QStringLiteral("🙂")
            << std::optional<Problem>();
        QTest::newRow("the longest")
            << QString(NewPassword::maxLength, QLatin1Char('a')) << std::optional<Problem>();

        QTest::newRow("empty") << "" << std::optional(Problem::Empty);
        QTest::newRow("one too long") << QString(NewPassword::maxLength + 1, QLatin1Char('a'))
                                      << std::optional(Problem::TooLong);
        QTest::newRow("a line break") << "one\ntwo" << std::optional(Problem::Unprintable);
        QTest::newRow("a carriage return") << "one\rtwo" << std::optional(Problem::Unprintable);
        QTest::newRow("a tab") << "one\ttwo" << std::optional(Problem::Unprintable);
        QTest::newRow("a nul") << QString(QChar(0)) << std::optional(Problem::Unprintable);
        QTest::newRow("an escape") << "a\x1b[2J" << std::optional(Problem::Unprintable);
        QTest::newRow("a lone surrogate")
            << QString(QChar(0xd800)) << std::optional(Problem::Unprintable);
    }

    void check() {
        QFETCH(const QString, password);
        QFETCH(const std::optional<Problem>, problem);
        QCOMPARE(NewPassword::check(password), problem);
    }
};

} // namespace

QTEST_GUILESS_MAIN(NewPasswordTest)
#include "tst_newpassword.moc"
