// SPDX-License-Identifier: Apache-2.0
#include "BatteryLook.h"

#include <QSignalSpy>
#include <QTest>

#include <limits>

namespace {

using Supply = BatteryLook::Supply;
using Warning = BatteryLook::Warning;

// UPower's codes, as BatteryLook reads them.
constexpr int typeUnknown = 0;
constexpr int typeBattery = 2;
constexpr int typeUps = 3;
constexpr int stateUnknown = 0;
constexpr int stateCharging = 1;
constexpr int stateDischarging = 2;
constexpr int stateEmpty = 3;
constexpr int stateFullyCharged = 4;
constexpr int statePendingCharge = 5;
constexpr int statePendingDischarge = 6;
constexpr int warningUnknown = 0;
constexpr int warningNone = 1;
constexpr int warningDischarging = 2;
constexpr int warningLow = 3;
constexpr int warningCritical = 4;
constexpr int warningAction = 5;

struct Reading {
    bool isPresent = true;
    double percentage = 42;
    int state = stateDischarging;
    int warningLevel = warningNone;
    int type = typeBattery;
};

void read(BatteryLook& look, const Reading& reading) {
    look.setIsPresent(reading.isPresent);
    look.setPercentage(reading.percentage);
    look.setState(reading.state);
    look.setWarningLevel(reading.warningLevel);
    look.setType(reading.type);
}

class BatteryLookTest : public QObject {
    Q_OBJECT

private slots:
    // A desktop or a virtual machine: what the VM's UPower reported on
    // 2026-10-18. Nothing shows, and the bar is as it was.
    void noBatteryShowsNothing() {
        BatteryLook look;
        read(look, {.isPresent = false,
                    .percentage = 0,
                    .state = stateUnknown,
                    .warningLevel = warningNone,
                    .type = typeUnknown});
        QCOMPARE(look.shown(), false);
        QCOMPARE(look.level(), 0);
        QCOMPARE(look.warning(), Warning::None);
    }

    // A UPS takes the batteries' place in UPower's DisplayDevice, and the
    // charger is not what a child would ask for.
    void aUpsShowsNothing() {
        BatteryLook look;
        read(look, {.percentage = 15,
                    .state = stateDischarging,
                    .warningLevel = warningLow,
                    .type = typeUps});
        QCOMPARE(look.shown(), false);
        QCOMPARE(look.warning(), Warning::None);
    }

    void aBatteryThatIsNotPresentShowsNothing() {
        BatteryLook look;
        read(look, {.isPresent = false, .percentage = 42, .type = typeBattery});
        QCOMPARE(look.shown(), false);
        QCOMPARE(look.level(), 0);
    }

    void aBatteryShowsHowFullItIs() {
        BatteryLook look;
        read(look, {});
        QCOMPARE(look.shown(), true);
        QCOMPARE(look.level(), 42);
        QCOMPARE(look.supply(), Supply::OnBattery);
        QCOMPARE(look.onCharger(), false);
        QCOMPARE(look.warning(), Warning::None);
    }

    void theLevelIsAWholeNumberFromEmptyToFull_data() {
        QTest::addColumn<double>("percentage");
        QTest::addColumn<int>("level");
        QTest::newRow("rounds up") << 42.6 << 43;
        QTest::newRow("rounds down") << 42.4 << 42;
        QTest::newRow("empty") << 0.0 << 0;
        QTest::newRow("full") << 100.0 << 100;
        QTest::newRow("above full") << 100.4 << 100;
        QTest::newRow("below empty") << -1.0 << 0;
        QTest::newRow("not a number") << std::numeric_limits<double>::quiet_NaN() << 0;
    }

    void theLevelIsAWholeNumberFromEmptyToFull() {
        QFETCH(const double, percentage);
        QFETCH(const int, level);
        BatteryLook look;
        read(look, {.percentage = percentage});
        QCOMPARE(look.level(), level);
    }

    void theChargerIsInOrNot_data() {
        QTest::addColumn<int>("state");
        QTest::addColumn<Supply>("supply");
        QTest::newRow("unknown") << stateUnknown << Supply::OnBattery;
        QTest::newRow("discharging") << stateDischarging << Supply::OnBattery;
        QTest::newRow("empty") << stateEmpty << Supply::OnBattery;
        QTest::newRow("pending discharge") << statePendingDischarge << Supply::OnBattery;
        QTest::newRow("charging") << stateCharging << Supply::Charging;
        QTest::newRow("fully charged") << stateFullyCharged << Supply::Full;
        // Plugged in and held, as by a charge limit.
        QTest::newRow("pending charge") << statePendingCharge << Supply::OnCharger;
    }

    void theChargerIsInOrNot() {
        QFETCH(const int, state);
        QFETCH(const Supply, supply);
        BatteryLook look;
        read(look, {.state = state});
        QCOMPARE(look.supply(), supply);
        QCOMPARE(look.onCharger(), supply != Supply::OnBattery);
    }

    void lowIsUPowersWarningLevel_data() {
        QTest::addColumn<int>("warningLevel");
        QTest::addColumn<Warning>("warning");
        QTest::newRow("unknown") << warningUnknown << Warning::None;
        QTest::newRow("none") << warningNone << Warning::None;
        // A UPS's level; a battery never has it.
        QTest::newRow("discharging") << warningDischarging << Warning::None;
        QTest::newRow("low") << warningLow << Warning::Low;
        QTest::newRow("critical") << warningCritical << Warning::AlmostEmpty;
        QTest::newRow("action") << warningAction << Warning::AlmostEmpty;
    }

    void lowIsUPowersWarningLevel() {
        QFETCH(const int, warningLevel);
        QFETCH(const Warning, warning);
        BatteryLook look;
        read(look, {.percentage = 4, .warningLevel = warningLevel});
        QCOMPARE(look.warning(), warning);
    }

    // Cairn has no threshold of its own: UPower's configuration decides.
    void theWarningIsUPowersNotOurs() {
        BatteryLook look;
        read(look, {.percentage = 12, .warningLevel = warningNone});
        QCOMPARE(look.warning(), Warning::None);
        read(look, {.percentage = 35, .warningLevel = warningLow});
        QCOMPARE(look.warning(), Warning::Low);
    }

    void onTheChargerNeverAWarning_data() {
        QTest::addColumn<int>("state");
        QTest::addColumn<int>("warningLevel");
        for (const int state : {stateCharging, stateFullyCharged, statePendingCharge}) {
            for (const int warningLevel : {warningLow, warningCritical, warningAction}) {
                QTest::addRow("state %d, warning level %d", state, warningLevel)
                    << state << warningLevel;
            }
        }
    }

    void onTheChargerNeverAWarning() {
        QFETCH(const int, state);
        QFETCH(const int, warningLevel);
        BatteryLook look;
        read(look, {.percentage = 4, .state = state, .warningLevel = warningLevel});
        QCOMPARE(look.onCharger(), true);
        QCOMPARE(look.warning(), Warning::None);
    }

    // A hidden battery has nothing to say, whatever the rest of the reading.
    void hiddenIsLevelNoneAndNothingToSay() {
        BatteryLook look;
        read(look, {.isPresent = false,
                    .percentage = 4,
                    .state = stateCharging,
                    .warningLevel = warningCritical});
        QCOMPARE(look.shown(), false);
        QCOMPARE(look.level(), 0);
        QCOMPARE(look.supply(), Supply::OnBattery);
        QCOMPARE(look.onCharger(), false);
        QCOMPARE(look.warning(), Warning::None);
    }

    // UPower sends a new percentage often; the bar redraws only when what
    // it shows changes.
    void theLookChangesOnlyWhenWhatItShowsChanges() {
        BatteryLook look;
        read(look, {});
        const QSignalSpy reading(&look, &BatteryLook::readingChanged);
        const QSignalSpy changed(&look, &BatteryLook::lookChanged);
        look.setPercentage(42.2);
        QCOMPARE(reading.count(), 1);
        QCOMPARE(changed.count(), 0);
        look.setPercentage(42.2);
        QCOMPARE(reading.count(), 1);
        look.setPercentage(std::numeric_limits<double>::quiet_NaN());
        look.setPercentage(std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(reading.count(), 2);
        QCOMPARE(changed.count(), 1);
        look.setPercentage(20);
        QCOMPARE(changed.count(), 2);
        look.setWarningLevel(warningLow);
        QCOMPARE(changed.count(), 3);
        QCOMPARE(look.warning(), Warning::Low);
        // Plugged in: the plug comes and the sentence goes, in one change.
        look.setState(stateCharging);
        QCOMPARE(changed.count(), 4);
        QCOMPARE(look.warning(), Warning::None);
        look.setWarningLevel(warningNone);
        QCOMPARE(changed.count(), 4);
    }
};

} // namespace

QTEST_GUILESS_MAIN(BatteryLookTest)
#include "tst_batterylook.moc"
