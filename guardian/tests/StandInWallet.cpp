// SPDX-License-Identifier: Apache-2.0
#include "StandInWallet.h"

#include <QDBusConnection>
#include <QLatin1StringView>
#include <QVariant>

#include <utility>

namespace {

constexpr auto connectionName = QLatin1StringView("kwalletd");
constexpr auto serviceName = QLatin1StringView("org.kde.kwalletd6");

QDBusConnection connection() {
    return QDBusConnection(connectionName);
}

} // namespace

bool StandInWallet::start() {
    QDBusConnection bus =
        QDBusConnection::connectToBus(QDBusConnection::SessionBus, connectionName);
    return bus.isConnected() &&
           bus.registerObject(QStringLiteral("/modules/kwalletd6"), this,
                              QDBusConnection::ExportAllSlots) &&
           comeBack();
}

bool StandInWallet::leave() {
    return connection().unregisterService(serviceName);
}

// Only a name no one holds: on a real session, with kwalletd running, this
// is refused rather than queued behind it.
bool StandInWallet::comeBack() {
    return connection().registerService(serviceName);
}

void StandInWallet::reset() {
    enabled = true;
    localWalletName = QStringLiteral("kdewallet");
    walletNames = {QStringLiteral("kdewallet")};
    failing.clear();
    odd.clear();
    holding = false;
    m_held.clear();
    asked.clear();
}

void StandInWallet::release() {
    holding = false;
    for (const QDBusMessage& call : std::as_const(m_held)) {
        connection().send(call.createReply(enabled));
    }
    m_held.clear();
}

bool StandInWallet::isEnabled(const QDBusMessage& call) {
    if (answeredOddly(QStringLiteral("isEnabled"), call)) {
        return false;
    }
    if (holding) {
        call.setDelayedReply(true);
        m_held.append(call);
        return false;
    }
    return enabled;
}

QString StandInWallet::localWallet(const QDBusMessage& call) {
    if (answeredOddly(QStringLiteral("localWallet"), call)) {
        return {};
    }
    return localWalletName;
}

QStringList StandInWallet::wallets(const QDBusMessage& call) {
    if (answeredOddly(QStringLiteral("wallets"), call)) {
        return {};
    }
    return walletNames;
}

void StandInWallet::changePassword(const QString& wallet, qlonglong window,
                                   const QString& application, const QDBusMessage& call) {
    asked.append(
        QStringLiteral("changePassword %1 %2 %3").arg(wallet).arg(window).arg(application));
    if (failing == QStringLiteral("changePassword")) {
        call.setDelayedReply(true);
        connection().send(call.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                                                QStringLiteral("The wallet is busy.")));
    }
}

// Qt sends a slot's return value as its answer, unless the call is told
// that its answer comes later; then the answer is sent here instead.
bool StandInWallet::answeredOddly(const QString& question, const QDBusMessage& call) {
    asked.append(question);
    if (failing == question) {
        call.setDelayedReply(true);
        connection().send(call.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.Failed"),
                                                QStringLiteral("kwalletd could not answer.")));
        return true;
    }
    if (odd == question) {
        call.setDelayedReply(true);
        connection().send(call.createReply(QVariant::fromValue(7)));
        return true;
    }
    return false;
}
