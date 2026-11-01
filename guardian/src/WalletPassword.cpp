// SPDX-License-Identifier: Apache-2.0
#include "WalletPassword.h"

#include <QCoreApplication>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QStringList>

WalletPassword::WalletPassword(const std::optional<QDBusConnection>& bus, QObject* parent)
    : QObject(parent), m_bus(bus ? *bus : QDBusConnection(QString())) {}

WalletPassword::Step WalletPassword::step() const {
    return m_step;
}

// Three questions, one after another, each answered without the window
// waiting: whether KWallet is on, the wallet pam_kwallet opens, and the
// wallets there are.
void WalletPassword::check() {
    if (!m_bus.isConnected() || m_step == Step::Checking || m_step == Step::Offered) {
        return;
    }
    setStep(Step::Checking);
    m_call = ask(QStringLiteral("isEnabled"));
    connect(m_call, &QDBusPendingCallWatcher::finished, this, &WalletPassword::enabledAnswered);
}

void WalletPassword::withdraw() {
    m_call = nullptr;
    setStep(Step::NotOffered);
}

// kwalletd takes a window to put its box over. A window on Wayland has no
// id another program can use, so it gets none, 0. This program's name is
// for kwalletd's own notes. No password goes with it.
void WalletPassword::change() {
    if (m_step != Step::Offered && m_step != Step::NoAnswer) {
        return;
    }
    setStep(Step::WalletAsks);
    m_call = ask(QStringLiteral("changePassword"),
                 {m_wallet, qlonglong{0}, QCoreApplication::applicationName()});
    connect(m_call, &QDBusPendingCallWatcher::finished, this, &WalletPassword::changeAnswered);
}

void WalletPassword::notNow() {
    if (m_step == Step::Offered || m_step == Step::NoAnswer) {
        withdraw();
    }
}

// kwalletd's own name, object and interface, which KWallet's library uses
// too. When kwalletd is not running, the bus starts it, unless KWallet is
// turned off; then the call fails.
QDBusPendingCallWatcher* WalletPassword::ask(const QString& method, const QVariantList& arguments) {
    QDBusMessage call = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kwalletd6"),
                                                       QStringLiteral("/modules/kwalletd6"),
                                                       QStringLiteral("org.kde.KWallet"), method);
    call.setArguments(arguments);
    return new QDBusPendingCallWatcher(m_bus.asyncCall(call), this);
}

// Each watcher goes once it has answered. Only the answer to the question
// asked last counts: withdrawing drops the one still to come.
bool WalletPassword::isAwaited(QDBusPendingCallWatcher* watcher) {
    watcher->deleteLater();
    if (watcher != m_call) {
        return false;
    }
    m_call = nullptr;
    return true;
}

// A typed reply is an error when kwalletd answers with another type, so an
// answer that is not plainly "on" offers nothing.
void WalletPassword::enabledAnswered(QDBusPendingCallWatcher* watcher) {
    if (!isAwaited(watcher)) {
        return;
    }
    const QDBusPendingReply<bool> enabled = *watcher;
    if (enabled.isError() || !enabled.value()) {
        setStep(Step::NotOffered);
        return;
    }
    m_call = ask(QStringLiteral("localWallet"));
    connect(m_call, &QDBusPendingCallWatcher::finished, this, &WalletPassword::nameAnswered);
}

void WalletPassword::nameAnswered(QDBusPendingCallWatcher* watcher) {
    if (!isAwaited(watcher)) {
        return;
    }
    const QDBusPendingReply<QString> name = *watcher;
    if (name.isError() || name.value().isEmpty()) {
        setStep(Step::NotOffered);
        return;
    }
    m_wallet = name.value();
    m_call = ask(QStringLiteral("wallets"));
    connect(m_call, &QDBusPendingCallWatcher::finished, this, &WalletPassword::walletsAnswered);
}

void WalletPassword::walletsAnswered(QDBusPendingCallWatcher* watcher) {
    if (!isAwaited(watcher)) {
        return;
    }
    const QDBusPendingReply<QStringList> wallets = *watcher;
    const bool hasIt = !wallets.isError() && wallets.value().contains(m_wallet);
    setStep(hasIt ? Step::Offered : Step::NotOffered);
}

// kwalletd answers at once, before its box is answered, so Family never
// learns whether the wallet's password changed: only whether kwalletd took
// the change.
void WalletPassword::changeAnswered(QDBusPendingCallWatcher* watcher) {
    if (!isAwaited(watcher)) {
        return;
    }
    const QDBusPendingReply<> reply = *watcher;
    if (reply.isError()) {
        qWarning("cairn-guardian: kwalletd did not take the change of the wallet's password: %s",
                 qUtf8Printable(reply.error().message()));
        setStep(Step::NoAnswer);
    }
}

void WalletPassword::setStep(Step step) {
    if (step == m_step) {
        return;
    }
    m_step = step;
    emit stepChanged();
}
