// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDBusConnection>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QVariantList>

#include <cstdint>
#include <optional>

class QDBusPendingCallWatcher;

// KDE Wallet's password, once the Guardian has changed their own on Your
// account (ADR-0043). At login pam_kwallet opens the wallet with the
// password typed, so the wallet opens by itself only while its password is
// the login password. As Plasma's Users page did, the page then offers to
// change the wallet's password too. It asks kwalletd over D-Bus, and only
// offers when KWallet is on and has the wallet pam_kwallet opens; any
// other answer, or none, means no offer. Family never sees a wallet
// password: kwalletd's own box asks for both.
class WalletPassword : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The Your account page has one.")
    Q_PROPERTY(Step step READ step NOTIFY stepChanged)

public:
    enum class Step : std::uint8_t {
        NotOffered,
        Checking, // kwalletd is asked whether there is a wallet to offer
        Offered,
        WalletAsks, // kwalletd took the change, and its own box asks
        NoAnswer,   // kwalletd did not take the change
    };
    Q_ENUM(Step)

    // kwalletd's bus, the Guardian's own session bus. Without one, nothing
    // is ever offered.
    explicit WalletPassword(const std::optional<QDBusConnection>& bus, QObject* parent = nullptr);

    Step step() const;

    // After the Guardian's password has changed: asks kwalletd, then offers
    // or not. Nothing while it asks or offers already.
    void check();
    // The sentence about the password change has gone, so the offer goes
    // too, and an answer still to come from kwalletd is dropped.
    void withdraw();
    // Asks kwalletd to change the wallet's password; its own box asks for
    // the passwords. Only while offered, or after kwalletd did not answer.
    Q_INVOKABLE void change();
    // Leaves the wallet as it is. Only while offered, or after kwalletd did
    // not answer.
    Q_INVOKABLE void notNow();

signals:
    void stepChanged();

private:
    QDBusPendingCallWatcher* ask(const QString& method, const QVariantList& arguments = {});
    bool isAwaited(QDBusPendingCallWatcher* watcher);
    void enabledAnswered(QDBusPendingCallWatcher* watcher);
    void nameAnswered(QDBusPendingCallWatcher* watcher);
    void walletsAnswered(QDBusPendingCallWatcher* watcher);
    void changeAnswered(QDBusPendingCallWatcher* watcher);
    void setStep(Step step);

    // A connection to nothing when no bus was given, whose calls fail.
    QDBusConnection m_bus;
    // The question kwalletd has not answered yet, or nullptr when there is
    // none or its answer is to be dropped.
    QDBusPendingCallWatcher* m_call = nullptr;
    // The wallet pam_kwallet opens at login, as kwalletd names it.
    QString m_wallet;
    Step m_step = Step::NotOffered;
};
