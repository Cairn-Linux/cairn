// SPDX-License-Identifier: Apache-2.0
#include "GrownUpLine.h"

AppLauncher::GrownUpStep GrownUpLine::step(AppLauncher::State state, const SteamGameWait& steam) {
    // A window that opened on its own, a coming-soon tile and a launch still
    // going get no line. Nor does a tile that is not a Steam game: its tap
    // made the wait forget what Steam's logs said (SteamGameWait).
    if (state != AppLauncher::State::Failed) {
        return AppLauncher::GrownUpStep::None;
    }
    if (steam.signInRefused()) {
        return AppLauncher::GrownUpStep::LogOutForAGrownUp;
    }
    if (steam.logOutWouldHelp()) {
        return AppLauncher::GrownUpStep::LogOutAndBackIn;
    }
    return AppLauncher::GrownUpStep::None;
}
