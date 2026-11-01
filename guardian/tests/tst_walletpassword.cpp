// SPDX-License-Identifier: Apache-2.0
#include "StandInWallet.h"
#include "WalletPassword.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <optional>

namespace {

constexpr int kAnswerMilliseconds = 10000;
// Long enough for kwalletd's answers to come, when nothing is to change.
constexpr int kQuietMilliseconds = 300;

// What a check asks kwalletd, in order.
QStringList threeQuestions() {
    return {"isEnabled", "localWallet", "wallets"};
}

class WalletPasswordTest : public QObject {
    Q_OBJECT

    StandInWallet m_kwalletd;

    static std::unique_ptr<WalletPassword> wallet() {
        return std::make_unique<WalletPassword>(QDBusConnection::sessionBus());
    }

    // A wallet whose check has had its answers.
    static std::unique_ptr<WalletPassword> checked() {
        std::unique_ptr<WalletPassword> password = wallet();
        password->check();
        static_cast<void>(QTest::qWaitFor(
            [&password] { return password->step() != WalletPassword::Step::Checking; },
            kAnswerMilliseconds));
        return password;
    }

    static QString changeOf(const QString& name) {
        return QStringLiteral("changePassword %1 0 %2")
            .arg(name, QCoreApplication::applicationName());
    }

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void offersWhenTheWalletIsThere() const;
    void offersTheWalletPamKwalletOpens();
    void noOffer_data();
    void noOffer();
    void noOfferWhenKwalletIsOff();
    void noOfferWithoutKwalletd();
    void noOfferWithoutABus() const;
    void oneCheckAtATime() const;
    void aWithdrawnCheckIsDropped() const;
    void changeHandsKwalletdTheWallet() const;
    void changeAndNotNowOnlyWhenOffered() const;
    void kwalletdDoesNotTakeTheChange();
    void notNowLeavesTheWallet();
    void aNewPasswordOffersAgain() const;
};

void WalletPasswordTest::initTestCase() {
    QVERIFY2(QDBusConnection::sessionBus().isConnected(),
             "run under dbus-run-session, as CTest does");
    QVERIFY2(m_kwalletd.start(), "kwalletd's name is taken: run under dbus-run-session, as "
                                 "CTest does, never on a real session");
}

void WalletPasswordTest::cleanupTestCase() {
    QDBusConnection::disconnectFromBus(QStringLiteral("kwalletd"));
}

void WalletPasswordTest::init() {
    m_kwalletd.reset();
}

// The three questions, one after another, and then the offer.
void WalletPasswordTest::offersWhenTheWalletIsThere() const {
    const std::unique_ptr<WalletPassword> password = wallet();
    const QSignalSpy stepChanged(password.get(), &WalletPassword::stepChanged);
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);
    password->check();
    QCOMPARE(password->step(), WalletPassword::Step::Checking);
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::Offered, kAnswerMilliseconds);
    QCOMPARE(m_kwalletd.asked, threeQuestions());
    QCOMPARE(stepChanged.count(), 2);
}

// The wallet pam_kwallet opens is the one kwalletd names, which is not
// kdewallet when kwalletrc's Use One Wallet is off; that one is changed.
void WalletPasswordTest::offersTheWalletPamKwalletOpens() {
    m_kwalletd.localWalletName = QStringLiteral("family");
    m_kwalletd.walletNames = {QStringLiteral("kdewallet"), QStringLiteral("family")};
    const std::unique_ptr<WalletPassword> password = checked();
    QCOMPARE(password->step(), WalletPassword::Step::Offered);
    password->change();
    QTRY_COMPARE_WITH_TIMEOUT(m_kwalletd.asked.size(), 4, kAnswerMilliseconds);
    QCOMPARE(m_kwalletd.asked.constLast(), changeOf(QStringLiteral("family")));
}

// No offer and nothing changed, whatever kwalletd says short of the
// wallet pam_kwallet opens being there.
void WalletPasswordTest::noOffer_data() {
    QTest::addColumn<QString>("localWallet");
    QTest::addColumn<QStringList>("wallets");
    QTest::addColumn<QString>("failing");
    QTest::addColumn<QString>("odd");

    const QString kdewallet = QStringLiteral("kdewallet");
    QTest::newRow("no wallet") << kdewallet << QStringList() << "" << "";
    QTest::newRow("another wallet only") << kdewallet << QStringList{"family"} << "" << "";
    QTest::newRow("no name for the wallet") << "" << QStringList{""} << "" << "";
    QTest::newRow("isEnabled fails") << kdewallet << QStringList{kdewallet} << "isEnabled" << "";
    QTest::newRow("localWallet fails")
        << kdewallet << QStringList{kdewallet} << "localWallet" << "";
    QTest::newRow("wallets fails") << kdewallet << QStringList{kdewallet} << "wallets" << "";
    QTest::newRow("isEnabled answers a number")
        << kdewallet << QStringList{kdewallet} << "" << "isEnabled";
    QTest::newRow("localWallet answers a number")
        << kdewallet << QStringList{kdewallet} << "" << "localWallet";
    QTest::newRow("wallets answers a number")
        << kdewallet << QStringList{kdewallet} << "" << "wallets";
}

void WalletPasswordTest::noOffer() {
    QFETCH(const QString, localWallet);
    QFETCH(const QStringList, wallets);
    QFETCH(const QString, failing);
    QFETCH(const QString, odd);
    m_kwalletd.localWalletName = localWallet;
    m_kwalletd.walletNames = wallets;
    m_kwalletd.failing = failing;
    m_kwalletd.odd = odd;
    const std::unique_ptr<WalletPassword> password = checked();
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);
    password->change();
    QTest::qWait(kQuietMilliseconds);
    QVERIFY(!m_kwalletd.asked.join(' ').contains(QStringLiteral("changePassword")));
}

// KWallet turned off answers the first question; nothing more is asked.
void WalletPasswordTest::noOfferWhenKwalletIsOff() {
    m_kwalletd.enabled = false;
    const std::unique_ptr<WalletPassword> password = checked();
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);
    QTest::qWait(kQuietMilliseconds);
    QCOMPARE(m_kwalletd.asked, QStringList{"isEnabled"});
}

// No kwalletd on the bus, as on a computer without KDE: the question
// fails at once, and nothing is offered.
void WalletPasswordTest::noOfferWithoutKwalletd() {
    QVERIFY(m_kwalletd.leave());
    const std::unique_ptr<WalletPassword> password = checked();
    const WalletPassword::Step step = password->step();
    QVERIFY(m_kwalletd.comeBack());
    QCOMPARE(step, WalletPassword::Step::NotOffered);
    QVERIFY(m_kwalletd.asked.isEmpty());
}

void WalletPasswordTest::noOfferWithoutABus() const {
    WalletPassword password(std::nullopt);
    password.check();
    QCOMPARE(password.step(), WalletPassword::Step::NotOffered);
    QTest::qWait(kQuietMilliseconds);
    QVERIFY(m_kwalletd.asked.isEmpty());
}

// A check while one runs, or once the offer is up, asks nothing again.
void WalletPasswordTest::oneCheckAtATime() const {
    const std::unique_ptr<WalletPassword> password = wallet();
    password->check();
    password->check();
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::Offered, kAnswerMilliseconds);
    password->check();
    QCOMPARE(password->step(), WalletPassword::Step::Offered);
    QTest::qWait(kQuietMilliseconds);
    QCOMPARE(m_kwalletd.asked, threeQuestions());
}

// Withdrawn while kwalletd has not answered: its answer is dropped, so no
// offer comes late and nothing more is asked. A new check then works.
void WalletPasswordTest::aWithdrawnCheckIsDropped() const {
    const std::unique_ptr<WalletPassword> password = wallet();
    password->check();
    password->withdraw();
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);
    QTRY_COMPARE_WITH_TIMEOUT(m_kwalletd.asked, QStringList{"isEnabled"}, kAnswerMilliseconds);
    QTest::qWait(kQuietMilliseconds);
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);
    QCOMPARE(m_kwalletd.asked, QStringList{"isEnabled"});

    password->check();
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::Offered, kAnswerMilliseconds);
}

// kwalletd gets the wallet's name, no window and this program's name, and
// no password; its own box asks for them.
void WalletPasswordTest::changeHandsKwalletdTheWallet() const {
    const std::unique_ptr<WalletPassword> password = checked();
    password->change();
    QCOMPARE(password->step(), WalletPassword::Step::WalletAsks);
    QTRY_COMPARE_WITH_TIMEOUT(m_kwalletd.asked.size(), 4, kAnswerMilliseconds);
    QCOMPARE(m_kwalletd.asked.constLast(), changeOf(QStringLiteral("kdewallet")));
    QTest::qWait(kQuietMilliseconds);
    QCOMPARE(password->step(), WalletPassword::Step::WalletAsks);
}

void WalletPasswordTest::changeAndNotNowOnlyWhenOffered() const {
    const std::unique_ptr<WalletPassword> password = wallet();
    password->change();
    password->notNow();
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);
    password->check();
    password->change();
    password->notNow();
    QCOMPARE(password->step(), WalletPassword::Step::Checking);
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::Offered, kAnswerMilliseconds);
    password->change();
    password->change();
    password->notNow();
    QCOMPARE(password->step(), WalletPassword::Step::WalletAsks);
    QTRY_COMPARE_WITH_TIMEOUT(m_kwalletd.asked.size(), 4, kAnswerMilliseconds);
    QTest::qWait(kQuietMilliseconds);
    QCOMPARE(m_kwalletd.asked, threeQuestions() + QStringList{changeOf("kdewallet")});
}

// A change kwalletd does not take is said, and can be tried again. One
// withdrawn meanwhile is dropped.
void WalletPasswordTest::kwalletdDoesNotTakeTheChange() {
    m_kwalletd.failing = QStringLiteral("changePassword");
    const std::unique_ptr<WalletPassword> password = checked();
    password->change();
    QCOMPARE(password->step(), WalletPassword::Step::WalletAsks);
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::NoAnswer,
                              kAnswerMilliseconds);

    m_kwalletd.failing.clear();
    password->change();
    QCOMPARE(password->step(), WalletPassword::Step::WalletAsks);
    QTRY_COMPARE_WITH_TIMEOUT(m_kwalletd.asked.count(changeOf(QStringLiteral("kdewallet"))), 2,
                              kAnswerMilliseconds);
    QTest::qWait(kQuietMilliseconds);
    QCOMPARE(password->step(), WalletPassword::Step::WalletAsks);

    m_kwalletd.failing = QStringLiteral("changePassword");
    password->check();
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::Offered, kAnswerMilliseconds);
    password->change();
    password->withdraw();
    QTest::qWait(kQuietMilliseconds);
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);
}

void WalletPasswordTest::notNowLeavesTheWallet() {
    const std::unique_ptr<WalletPassword> password = checked();
    password->notNow();
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);

    m_kwalletd.failing = QStringLiteral("changePassword");
    password->check();
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::Offered, kAnswerMilliseconds);
    password->change();
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::NoAnswer,
                              kAnswerMilliseconds);
    password->notNow();
    QCOMPARE(password->step(), WalletPassword::Step::NotOffered);
    QCOMPARE(m_kwalletd.asked.count(changeOf(QStringLiteral("kdewallet"))), 1);
}

// The Guardian changes their password again after kwalletd's box opened:
// the wallet's password differs again, so the offer comes again.
void WalletPasswordTest::aNewPasswordOffersAgain() const {
    const std::unique_ptr<WalletPassword> password = checked();
    password->change();
    QCOMPARE(password->step(), WalletPassword::Step::WalletAsks);
    password->check();
    QCOMPARE(password->step(), WalletPassword::Step::Checking);
    QTRY_COMPARE_WITH_TIMEOUT(password->step(), WalletPassword::Step::Offered, kAnswerMilliseconds);
    QCOMPARE(m_kwalletd.asked.count(QStringLiteral("isEnabled")), 2);
}

} // namespace

QTEST_MAIN(WalletPasswordTest)
#include "tst_walletpassword.moc"
