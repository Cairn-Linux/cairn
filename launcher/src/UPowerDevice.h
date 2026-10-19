// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// UPower's DisplayDevice as plain properties, kept current (ADR-0034).
// UPower already puts a laptop's batteries together into one device and
// decides when it is low, so the launcher asks UPower on the system bus
// instead of reading the battery itself. This class decides nothing:
// BatteryLook does.
//
// It reads IsPresent, Percentage, State, WarningLevel and Type from
// org.freedesktop.UPower.Device, follows PropertiesChanged, and reads them
// all again when UPower restarts. It never changes anything in UPower.
// With no bus or no UPower it reads as no battery. After a failed read it
// reads as no battery until UPower's next change, then reads it all again.
class UPowerDevice : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // The bus name to ask. Empty, the default, asks nothing, so a window
    // made in a test never reads the computer's own battery.
    Q_PROPERTY(QString service READ service WRITE setService NOTIFY serviceChanged)
    // UPower's names and codes; D-Bus's unsigned values arrive as int.
    Q_PROPERTY(bool isPresent READ isPresent NOTIFY readingChanged)
    Q_PROPERTY(double percentage READ percentage NOTIFY readingChanged)
    Q_PROPERTY(int state READ state NOTIFY readingChanged)
    Q_PROPERTY(int warningLevel READ warningLevel NOTIFY readingChanged)
    Q_PROPERTY(int type READ type NOTIFY readingChanged)

public:
    // The system bus and UPower's DisplayDevice.
    explicit UPowerDevice(QObject* parent = nullptr);
    // Any bus and path, so a test can stand in for UPower on a bus of its own.
    UPowerDevice(QDBusConnection bus, QString path, QObject* parent = nullptr);

    // org.freedesktop.UPower, UPower's name on the system bus.
    static QString systemService();

    QString service() const;
    void setService(const QString& service);
    bool isPresent() const;
    double percentage() const;
    int state() const;
    int warningLevel() const;
    int type() const;

signals:
    void serviceChanged();
    void readingChanged();

private:
    // QtDBus finds it by its name, so it has to be a slot.
    Q_SLOT void onPropertiesChanged(const QString& interface, const QVariantMap& changed,
                                    const QStringList& invalidated);
    bool follow();
    void unfollow();
    void readAll();
    void take(const QVariantMap& properties);
    void forget();

    QDBusConnection m_bus;
    QString m_path;
    QString m_service;
    QDBusServiceWatcher m_watcher;
    bool m_isPresent = false;
    double m_percentage = 0;
    int m_state = 0;
    int m_warningLevel = 0;
    int m_type = 0;
    // True once a GetAll answer has been taken. Until then a change alone
    // is not enough: Type never changes, so UPower never sends it again.
    bool m_readInFull = false;
};
