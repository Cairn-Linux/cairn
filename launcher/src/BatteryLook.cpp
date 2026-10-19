// SPDX-License-Identifier: Apache-2.0
#include "BatteryLook.h"

#include <algorithm>
#include <cmath>

namespace {

// UPower's codes for org.freedesktop.UPower.Device, as UPower 1.91 sends
// them. Only the ones this class tells apart are named.
constexpr int typeBattery = 2;
constexpr int stateCharging = 1;
constexpr int stateFullyCharged = 4;
constexpr int statePendingCharge = 5;
constexpr int warningLow = 3;
constexpr int warningCritical = 4;
constexpr int warningAction = 5;

} // namespace

BatteryLook::BatteryLook(QObject* parent) : QObject(parent) {}

bool BatteryLook::isPresent() const {
    return m_isPresent;
}

void BatteryLook::setIsPresent(bool isPresent) {
    if (isPresent == m_isPresent) {
        return;
    }
    m_isPresent = isPresent;
    emit readingChanged();
    decide();
}

double BatteryLook::percentage() const {
    return m_percentage;
}

void BatteryLook::setPercentage(double percentage) {
    // NaN is never equal to itself, so it is compared by kind.
    const bool same =
        std::isnan(percentage) ? std::isnan(m_percentage) : percentage == m_percentage;
    if (same) {
        return;
    }
    m_percentage = percentage;
    emit readingChanged();
    decide();
}

int BatteryLook::state() const {
    return m_state;
}

void BatteryLook::setState(int state) {
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit readingChanged();
    decide();
}

int BatteryLook::warningLevel() const {
    return m_warningLevel;
}

void BatteryLook::setWarningLevel(int warningLevel) {
    if (warningLevel == m_warningLevel) {
        return;
    }
    m_warningLevel = warningLevel;
    emit readingChanged();
    decide();
}

int BatteryLook::type() const {
    return m_type;
}

void BatteryLook::setType(int type) {
    if (type == m_type) {
        return;
    }
    m_type = type;
    emit readingChanged();
    decide();
}

bool BatteryLook::shown() const {
    return m_shown;
}

int BatteryLook::level() const {
    return m_level;
}

BatteryLook::Supply BatteryLook::supply() const {
    return m_supply;
}

bool BatteryLook::onCharger() const {
    return m_supply != Supply::OnBattery;
}

BatteryLook::Warning BatteryLook::warning() const {
    return m_warning;
}

void BatteryLook::decide() {
    // A computer with no battery reports type 0 and IsPresent false. A UPS
    // replaces the batteries in UPower's DisplayDevice, and "ask a grown-up
    // for the charger" would be wrong for one, so only a battery shows.
    const bool shown = m_isPresent && m_type == typeBattery;

    int level = 0;
    if (shown && !std::isnan(m_percentage)) {
        level = static_cast<int>(std::lround(std::clamp(m_percentage, 0.0, 100.0)));
    }

    Supply supply = Supply::OnBattery;
    if (shown) {
        switch (m_state) {
        case stateCharging:
            supply = Supply::Charging;
            break;
        case stateFullyCharged:
            supply = Supply::Full;
            break;
        case statePendingCharge:
            supply = Supply::OnCharger;
            break;
        default:
            // Discharging, empty, pending discharge and unknown: nothing
            // says the charger is in.
            break;
        }
    }

    // UPower says low only while discharging. A child plugging the charger
    // in changes State and WarningLevel in two steps, and the sentence goes
    // with the first. Critical and action say the same: what UPower does at
    // the action level is its configuration's to decide, so the words
    // promise nothing about sleeping.
    Warning warning = Warning::None;
    if (shown && supply == Supply::OnBattery) {
        if (m_warningLevel == warningLow) {
            warning = Warning::Low;
        } else if (m_warningLevel == warningCritical || m_warningLevel == warningAction) {
            warning = Warning::AlmostEmpty;
        }
    }

    if (shown == m_shown && level == m_level && supply == m_supply && warning == m_warning) {
        return;
    }
    m_shown = shown;
    m_level = level;
    m_supply = supply;
    m_warning = warning;
    emit lookChanged();
}
