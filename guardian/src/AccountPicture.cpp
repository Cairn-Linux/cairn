// SPDX-License-Identifier: Apache-2.0
#include "AccountPicture.h"

#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusVariant>
#include <QFileInfo>

#include <utility>

namespace {

QString accounts() {
    return QStringLiteral("org.freedesktop.Accounts");
}

QString userInterface() {
    return QStringLiteral("org.freedesktop.Accounts.User");
}

} // namespace

AccountPicture::AccountPicture(QDBusConnection bus, unsigned int uid, QObject* parent)
    : QObject(parent), m_bus(std::move(bus)), m_uid(uid) {}

// FindUserById answers with the account's object, or an error when there
// is no such account.
QString AccountPicture::userPath() const {
    QDBusMessage call =
        QDBusMessage::createMethodCall(accounts(), QStringLiteral("/org/freedesktop/Accounts"),
                                       accounts(), QStringLiteral("FindUserById"));
    call << static_cast<qint64>(m_uid);
    const QDBusReply<QDBusObjectPath> reply = m_bus.call(call);
    return reply.isValid() ? reply.value().path() : QString();
}

QString AccountPicture::current() const {
    const QString path = userPath();
    if (path.isEmpty()) {
        return {};
    }
    QDBusMessage call = QDBusMessage::createMethodCall(
        accounts(), path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"));
    call << userInterface() << QStringLiteral("IconFile");
    const QDBusReply<QDBusVariant> reply = m_bus.call(call);
    if (!reply.isValid()) {
        return {};
    }
    // AccountsService names ~/.face when it has no picture, whether or not
    // that file is there.
    const QString file = reply.value().variant().toString();
    return file.startsWith(QLatin1Char('/')) && QFileInfo::exists(file) ? file : QString();
}

bool AccountPicture::running() const {
    return m_call != nullptr;
}

bool AccountPicture::set(const QString& file) {
    if (running()) {
        return false;
    }
    const QString path = userPath();
    if (path.isEmpty()) {
        qWarning("cairn-guardian: AccountsService does not know this account, so the picture "
                 "stays as it was.");
        return false;
    }
    QDBusMessage call = QDBusMessage::createMethodCall(accounts(), path, userInterface(),
                                                       QStringLiteral("SetIconFile"));
    call << file;
    // A polkit rule may still ask for a password; the box then opens and
    // the window waits for it.
    call.setInteractiveAuthorizationAllowed(true);
    m_call = new QDBusPendingCallWatcher(m_bus.asyncCall(call), this);
    connect(m_call, &QDBusPendingCallWatcher::finished, this, &AccountPicture::answered);
    return true;
}

void AccountPicture::answered(QDBusPendingCallWatcher* watcher) {
    const QDBusPendingReply<> reply = *watcher;
    if (reply.isError()) {
        qWarning("cairn-guardian: AccountsService did not take the picture: %s",
                 qUtf8Printable(reply.error().message()));
    }
    watcher->deleteLater();
    m_call = nullptr;
    emit finished(!reply.isError());
}
