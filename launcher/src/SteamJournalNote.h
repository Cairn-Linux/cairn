// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamLaunchLog.h"
#include "SteamSignIn.h"

#include <QString>

// The journal's sentence for a Steam game that put nothing on the screen,
// from what Steam's logs said at the launch's last look (#182, #206, #211).
//
// The reason comes first, then what will help when the launcher knows. A
// refused sign-in comes before any other reason: nothing else helps until a
// grown-up signs Steam in again, from their own login once the child has
// logged out (ADR-0049). The grown-up screen's own line is GrownUpLine's.
class SteamJournalNote {
public:
    // What Steam's logs said at the launch's last look (SteamGameWait).
    struct Said {
        // Steam's servers turned its sign-in down, whatever else it said.
        bool signInRefused = false;
        // Whether Steam could sign in, as far as it decides the wait.
        SteamSignIn::Now signIn = SteamSignIn::Now::Fine;
        // What Steam's launch log said about the game.
        SteamLaunchLog::Step lastStep = SteamLaunchLog::Step::Nothing;
        // Logging the child out frees a game the cloud question holds.
        bool logOutFreesHeldGame = false;
        // SteamStall's own sentence, or empty.
        QString stallNote;
    };

    // The sentence for a game called `title`, or empty when Steam said
    // nothing that explains it.
    static QString of(const QString& title, const Said& said);
};
