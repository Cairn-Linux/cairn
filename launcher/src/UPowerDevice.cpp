// SPDX-License-Identifier: Apache-2.0
#include "UPowerDevice.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>

#include <utility>

namespace {

constexpr auto upowerService = QLatin1StringView("org.freedesktop.UPower");
constexpr auto displayDevicePath =
    QLatin1StringView("/org/freedesktop/UPower/devices/DisplayDevice");
constexpr auto deviceInterface = QLatin1StringView("org.freedesktop.UPower.Device");
constexpr auto propertiesInterface = QLatin1StringView("org.freedesktop.DBus.Properties");
constexpr auto propertiesChanged = QLatin1StringView("PropertiesChanged");

} // namespace

UPowerDevice::UPowerDevice(QObject* parent)
    : UPowerDevice(QDBusConnection::systemBus(), displayDevicePath, parent) {}

UPowerDevice::UPowerDevice(QDBusConnection bus, QString path, QObject* parent)
    : QObject(parent), m_bus(std::move(bus)), m_path(std::move(path)), m_watcher(this) {
    // UPower restarting is a new owner of its name: an empty one while it
    // is gone, then the new process.
    m_watcher.setConnection(m_bus);
    m_watcher.setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    connect(&m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString&, const QString&, const QString& newOwner) {
                if (newOwner.isEmpty()) {
                    forget();
                } else {
                    readAll();
                }
            });
}

QString UPowerDevice::systemService() {
    return upowerService;
}

QString UPowerDevice::service() const {
    return m_service;
}

void UPowerDevice::setService(const QString& service) {
    if (service == m_service) {
        return;
    }
    unfollow();
    m_service = service;
    emit serviceChanged();
    forget();
    if (service.isEmpty()) {
        return;
    }
    // On a bus that is not there a call fails at once, and its watcher
    // never says so, so the reading would wait for ever without this.
    if (!m_bus.isConnected()) {
        qWarning().noquote() << QStringLiteral("No system bus, so the launcher shows no battery.");
        return;
    }
    // A battery that never changes would be worse than none.
    if (!follow()) {
        qWarning().noquote() << QStringLiteral(
            "Could not follow the battery in UPower, so the launcher shows no battery.");
        return;
    }
    readAll();
}

bool UPowerDevice::isPresent() const {
    return m_isPresent;
}

double UPowerDevice::percentage() const {
    return m_percentage;
}

int UPowerDevice::state() const {
    return m_state;
}

int UPowerDevice::warningLevel() const {
    return m_warningLevel;
}

int UPowerDevice::type() const {
    return m_type;
}

void UPowerDevice::onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                       const QStringList& invalidated) {
    if (interface != deviceInterface) {
        return;
    }
    // An invalidated property comes without its value, and a change after a
    // failed read leaves the others unknown, so ask for them all. The answer
    // carries this change too.
    if (!invalidated.isEmpty() || !m_readInFull) {
        readAll();
        return;
    }
    take(changed);
}

// Only from the program that owns the service's name: QtDBus matches the
// signal against that owner, so another program on the bus cannot send the
// launcher a battery.
bool UPowerDevice::follow() {
    if (!m_bus.connect(m_service, m_path, propertiesInterface, propertiesChanged, this,
                       SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)))) {
        return false;
    }
    m_watcher.setWatchedServices({m_service});
    return true;
}

void UPowerDevice::unfollow() {
    m_watcher.setWatchedServices({});
    if (!m_service.isEmpty()) {
        m_bus.disconnect(m_service, m_path, propertiesInterface, propertiesChanged, this,
                         SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    }
}

void UPowerDevice::readAll() {
    QDBusMessage call = QDBusMessage::createMethodCall(m_service, m_path, propertiesInterface,
                                                       QStringLiteral("GetAll"));
    call << QString(deviceInterface);
    auto* watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call), this);
    // A reply for a service the launcher has stopped asking is not a reading.
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, asked = m_service](QDBusPendingCallWatcher* finished) {
                finished->deleteLater();
                if (asked != m_service) {
                    return;
                }
                const QDBusPendingReply<QVariantMap> reply = *finished;
                if (reply.isError()) {
                    qWarning().noquote()
                        << QStringLiteral("Could not read the battery from UPower (%1), so the "
                                          "launcher shows no battery.")
                               .arg(reply.error().name());
                    forget();
                    return;
                }
                m_readInFull = true;
                take(reply.value());
            });
}

// Takes the properties that are there and keeps the others: a change
// names only what changed.
void UPowerDevice::take(const QVariantMap& properties) {
    const bool isPresent = properties.value(QStringLiteral("IsPresent"), m_isPresent).toBool();
    const double percentage =
        properties.value(QStringLiteral("Percentage"), m_percentage).toDouble();
    const int state = properties.value(QStringLiteral("State"), m_state).toInt();
    const int warningLevel =
        properties.value(QStringLiteral("WarningLevel"), m_warningLevel).toInt();
    const int type = properties.value(QStringLiteral("Type"), m_type).toInt();
    if (isPresent == m_isPresent && percentage == m_percentage && state == m_state &&
        warningLevel == m_warningLevel && type == m_type) {
        return;
    }
    m_isPresent = isPresent;
    m_percentage = percentage;
    m_state = state;
    m_warningLevel = warningLevel;
    m_type = type;
    emit readingChanged();
}

void UPowerDevice::forget() {
    m_readInFull = false;
    take({{QStringLiteral("IsPresent"), false},
          {QStringLiteral("Percentage"), 0.0},
          {QStringLiteral("State"), 0},
          {QStringLiteral("WarningLevel"), 0},
          {QStringLiteral("Type"), 0}});
}
