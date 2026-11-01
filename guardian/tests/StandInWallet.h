// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDBusMessage>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

// A stand-in for kwalletd, with the four methods of its org.kde.KWallet
// interface that Family calls (ADR-0043). It answers as its members say,
// and writes down what it was asked. It answers on a connection of its
// own, from the thread it lives in: Family's calls do not wait for their
// answers, so it can live in the test's own thread.
class StandInWallet : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KWallet")

public:
    // What kwalletd answers.
    bool enabled = true;
    QString localWalletName = QStringLiteral("kdewallet");
    QStringList walletNames{QStringLiteral("kdewallet")};
    // A method that answers with an error, and one that answers with a
    // number, the wrong type, or "" for none.
    QString failing;
    QString odd;
    // Holds its answer to isEnabled until release(), so a test sees the
    // check still running.
    bool holding = false;
    // What it was asked, in order: each method's name, and for
    // changePassword its arguments too, as in
    // "changePassword kdewallet 0 tst_walletpassword".
    QStringList asked;

    // Takes kwalletd's name and object on the session bus. False when it
    // cannot, as on a real session where kwalletd is running.
    bool start();
    // Gives kwalletd's name up, as on a computer without KDE, or takes it
    // back.
    bool leave();
    bool comeBack();
    // A wallet that is there, every answer plain and given at once,
    // nothing asked yet.
    void reset();
    // Gives the answers it held, and holds no more.
    void release();

    // NOLINTNEXTLINE(readability-redundant-access-specifiers): Qt's slots.
public slots:
    bool isEnabled(const QDBusMessage& call);
    QString localWallet(const QDBusMessage& call);
    QStringList wallets(const QDBusMessage& call);
    void changePassword(const QString& wallet, qlonglong window, const QString& application,
                        const QDBusMessage& call);

private:
    // Writes the question down; true when it was answered here instead,
    // with an error or a number.
    bool answeredOddly(const QString& question, const QDBusMessage& call);

    QList<QDBusMessage> m_held;
};
