// SPDX-License-Identifier: Apache-2.0
#include "LoginName.h"

#include <QRegularExpression>
#include <QSet>
#include <QTest>

namespace {

// cairn-family's own rule for a login name (ADR-0039).
bool isPlain(const QString& login) {
    static const QRegularExpression plain(QStringLiteral("^[a-z_][a-z0-9_-]{0,31}$"));
    return plain.match(login).hasMatch();
}

class LoginNameTest : public QObject {
    Q_OBJECT

private slots:
    void fromName_data() {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("login");

        QTest::newRow("plain") << "Ada" << "ada";
        QTest::newRow("already a login") << "ben" << "ben";
        QTest::newRow("accents") << QStringLiteral("Zoë Ångström") << "zoeangstrom";
        QTest::newRow("cedilla and tilde") << QStringLiteral("François Núñez") << "francoisnunez";
        QTest::newRow("spaces dropped") << "Mary Ann" << "maryann";
        QTest::newRow("hyphen and underscore kept") << "Jean-Luc_B" << "jean-luc_b";
        QTest::newRow("digits after a letter kept") << "Ben 2" << "ben2";
        QTest::newRow("digits first dropped") << "2 Ben" << "ben";
        QTest::newRow("hyphen first dropped") << "-ada" << "ada";
        QTest::newRow("underscore may start") << "_ada" << "_ada";
        QTest::newRow("punctuation dropped") << "O'Brien, Ada!" << "obrienada";
        QTest::newRow("letters with no plain form dropped") << QStringLiteral("Straße") << "strae";
        QTest::newRow("cut at 32")
            << QString(40, QLatin1Char('a')) << QString(32, QLatin1Char('a'));
    }

    void fromName() {
        QFETCH(const QString, name);
        QFETCH(const QString, login);
        const std::optional<QString> made = loginFromName(name);
        QVERIFY(made.has_value());
        QCOMPARE(made.value_or(QString()), login);
        QVERIFY(isPlain(made.value_or(QString())));
    }

    // Nothing usable is left, so the window asks for a short name.
    void nothingLeft_data() {
        QTest::addColumn<QString>("name");
        QTest::newRow("empty") << QString();
        QTest::newRow("spaces") << "   ";
        QTest::newRow("digits only") << "2024";
        QTest::newRow("another alphabet") << QStringLiteral("Анна");
        QTest::newRow("another script") << QStringLiteral("さくら");
        QTest::newRow("punctuation") << "-- !";
    }

    void nothingLeft() {
        QFETCH(const QString, name);
        QVERIFY(!loginFromName(name).has_value());
    }

    void aFreeLoginIsKept() {
        QCOMPARE(freeLogin("ada", [](const QString&) { return false; }).value_or(QString()),
                 QStringLiteral("ada"));
    }

    // Any account or group with the name counts, so the next number is
    // tried until one is free.
    void aTakenLoginGetsANumber() {
        const QSet<QString> taken{"ada", "ada2", "ada3"};
        const auto inUse = [&taken](const QString& name) { return taken.contains(name); };
        QCOMPARE(freeLogin("ada", inUse).value_or(QString()), QStringLiteral("ada4"));
        QCOMPARE(freeLogin("ben", inUse).value_or(QString()), QStringLiteral("ben"));
    }

    void aLongLoginIsCutForItsNumber() {
        const QString longest(32, QLatin1Char('a'));
        const auto inUse = [&longest](const QString& name) { return name == longest; };
        const std::optional<QString> free = freeLogin(longest, inUse);
        QCOMPARE(free.value_or(QString()), QString(31, QLatin1Char('a')) + "2");
        QVERIFY(isPlain(free.value_or(QString())));
    }

    void everyNumberTaken() {
        QVERIFY(!freeLogin("ada", [](const QString&) { return true; }).has_value());
    }
};

} // namespace

QTEST_APPLESS_MAIN(LoginNameTest)
#include "tst_loginname.moc"
