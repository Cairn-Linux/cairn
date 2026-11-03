// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "AppLauncher.h"
#include "SteamGameWait.h"

// Which one line the grown-up screen adds under "<game> did not start.", if
// any (ADR-0045, ADR-0049).
//
// A line comes only after a failed launch of a Steam game, and only where the
// launcher knows what will help. When Steam's servers turned its sign-in
// down, nothing helps until a grown-up signs it in again. Cairn never asks
// for a grown-up's password inside a child's session, so the grown-up does
// that from their own login once the child has logged out, in a visit
// (ADR-0032). That line wins over the others, whatever else Steam's logs
// said, such as a question it holds or a launch it stopped part-way. A Steam
// that cannot reach its servers gets no line: it tries again by itself, and
// logging out does not bring the Wi-Fi back.
class GrownUpLine {
public:
    // The line for a launch that is in `state`, from what `steam` read in
    // Steam's logs at the launch's last look.
    static AppLauncher::GrownUpStep step(AppLauncher::State state, const SteamGameWait& steam);
};
