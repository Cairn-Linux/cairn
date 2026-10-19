// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>

#include <cstdint>

// Decides what the bar above the tiles shows for one reading of UPower's
// DisplayDevice (ADR-0034). On the kids' laptop a child played until the
// battery was empty, and nothing on the screen said it was running down.
//
// The reading is UPower's own, under UPower's names and codes, so this class
// can be tested without D-Bus. What counts as low is UPower's too: its
// WarningLevel follows /etc/UPower/UPower.conf, and Cairn sets no threshold
// of its own. This class only turns a reading into what a child sees: how
// full the battery is, whether it is on the charger, and whether to say it
// is low.
class BatteryLook : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // The reading: IsPresent, Percentage, State, WarningLevel and Type.
    Q_PROPERTY(bool isPresent READ isPresent WRITE setIsPresent NOTIFY readingChanged)
    Q_PROPERTY(double percentage READ percentage WRITE setPercentage NOTIFY readingChanged)
    Q_PROPERTY(int state READ state WRITE setState NOTIFY readingChanged)
    Q_PROPERTY(int warningLevel READ warningLevel WRITE setWarningLevel NOTIFY readingChanged)
    Q_PROPERTY(int type READ type WRITE setType NOTIFY readingChanged)
    // The decision. A hidden battery is level 0, on battery, with no warning.
    Q_PROPERTY(bool shown READ shown NOTIFY lookChanged)
    // How full, from 0 to 100.
    Q_PROPERTY(int level READ level NOTIFY lookChanged)
    Q_PROPERTY(Supply supply READ supply NOTIFY lookChanged)
    Q_PROPERTY(bool onCharger READ onCharger NOTIFY lookChanged)
    Q_PROPERTY(Warning warning READ warning NOTIFY lookChanged)

public:
    // OnCharger is plugged in but not charging, as when a charge limit
    // holds the battery below full.
    enum class Supply : std::uint8_t { OnBattery, Charging, OnCharger, Full };
    Q_ENUM(Supply)
    enum class Warning : std::uint8_t { None, Low, AlmostEmpty };
    Q_ENUM(Warning)

    explicit BatteryLook(QObject* parent = nullptr);

    bool isPresent() const;
    void setIsPresent(bool isPresent);
    double percentage() const;
    void setPercentage(double percentage);
    int state() const;
    void setState(int state);
    int warningLevel() const;
    void setWarningLevel(int warningLevel);
    int type() const;
    void setType(int type);

    bool shown() const;
    int level() const;
    Supply supply() const;
    bool onCharger() const;
    Warning warning() const;

signals:
    void readingChanged();
    void lookChanged();

private:
    void decide();

    bool m_isPresent = false;
    double m_percentage = 0;
    int m_state = 0;
    int m_warningLevel = 0;
    int m_type = 0;

    bool m_shown = false;
    int m_level = 0;
    Supply m_supply = Supply::OnBattery;
    Warning m_warning = Warning::None;
};
