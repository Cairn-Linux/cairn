// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "WalletPassword.h"

#include <QDBusConnection>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>
#include <QVariantList>

#include <cstdint>
#include <functional>
#include <optional>

class AccountPicture;
class FamilyChange;

// What the Guardian tool's "Your account" page shows and does: the
// grown-up's own name and picture, a new picture, and a new password
// (ADR-0043). Plasma's Users page, where a grown-up did both before, is
// hidden on Cairn. The password goes through cairn-family
// set-own-password under pkexec, which asks for the password they have
// now; the picture goes to AccountsService. Once a new password is in
// place, the page offers to change KDE Wallet's too, as the Users page
// did.
class AccountPage : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The window is given one by main().")
    Q_PROPERTY(QString name READ name CONSTANT)
    // The picture as a URL the page can show, changed after each new
    // picture so the page does not keep the old one; empty when none.
    Q_PROPERTY(QUrl picture READ picture NOTIFY pictureChanged)
    // KDE's own pictures to choose from, each as a map: url, and name, its
    // file's name without the end, such as "Konqi". First the PNG and JPEG
    // files in Plasma's folder, then those in its photos folder, each in
    // name order. The image leaves only KDE's there (ADR-0043).
    Q_PROPERTY(QVariantList pictures READ pictures CONSTANT)
    Q_PROPERTY(int maxPasswordLength READ maxPasswordLength CONSTANT)
    Q_PROPERTY(Step step READ step NOTIFY stepChanged)
    Q_PROPERTY(Reason reason READ reason NOTIFY stepChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stepChanged)
    // The offer to change KDE Wallet's password after a new password.
    Q_PROPERTY(WalletPassword* wallet READ wallet CONSTANT)

public:
    enum class Step : std::uint8_t {
        Ready,
        Asking, // pkexec asks for the current password, then the command runs
        PasswordChanged,
        PasswordNotChanged, // see reason()
        SettingPicture,
        PictureChanged,
        PictureNotChanged, // see reason()
    };
    Q_ENUM(Step)

    // Why nothing changed. The first four the page finds itself; then
    // cairn-family's words (ADR-0039, ADR-0043) and pkexec's; then the
    // picture's two.
    enum class Reason : std::uint8_t {
        None,
        Empty,
        Different,
        TooLong,
        Unprintable,
        NotRoot,
        NotAGrownUp,
        Usage,
        BadPassword,
        Failed,
        Dismissed,
        NotAuthorized,
        Unknown,
        PictureUnreadable,
        PictureRefused,
    };
    Q_ENUM(Reason)

    struct Setup {
        QString pkexec = QStringLiteral("/usr/bin/pkexec");
        // cairn-family; main() names the one this build installs.
        QString command;
        // The grown-up's own account; main() gives the real one.
        unsigned int uid = 0;
        std::function<std::optional<Account>(unsigned int)> lookUpUid = lookUpAccountByUid;
        // AccountsService's bus. Without one, the picture cannot change.
        std::optional<QDBusConnection> bus;
        // Plasma's picture folder, with KDE's photos in its photos folder.
        QString stockPictures = QStringLiteral("/usr/share/plasma/avatars");
        // kwalletd's bus, the grown-up's own session bus. Without one, the
        // page never offers to change the wallet's password.
        std::optional<QDBusConnection> walletBus;
    };

    explicit AccountPage(Setup setup, QObject* parent = nullptr);

    QString name() const;
    QUrl picture() const;
    QVariantList pictures() const;
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    int maxPasswordLength() const;
    Step step() const;
    Reason reason() const;
    bool busy() const;
    WalletPassword* wallet() const;

    Q_INVOKABLE void changePassword(const QString& password, const QString& again);
    Q_INVOKABLE void choosePicture(const QUrl& file);
    // Reads the picture again from AccountsService.
    Q_INVOKABLE void reload();

    static Reason reasonFor(const QString& answer);

signals:
    void pictureChanged();
    void stepChanged();

private:
    void passwordAnswered(const QString& answer);
    void pictureAnswered(bool changed);
    void setStep(Step step, Reason reason = Reason::None);

    Setup m_setup;
    FamilyChange* m_change;
    WalletPassword* m_wallet;
    AccountPicture* m_picture = nullptr;
    // Where a chosen picture is made ready, removed with the page.
    QTemporaryDir m_work;
    QString m_name;
    QString m_pictureFile;
    int m_pictureVersion = 0;
    Step m_step = Step::Ready;
    Reason m_reason = Reason::None;
};
