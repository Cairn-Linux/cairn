// SPDX-License-Identifier: Apache-2.0
#include "SteamJournalNote.h"

QString SteamJournalNote::of(const QString& title, const Said& said) {
    if (said.signInRefused) {
        return QStringLiteral("Steam could not sign in, so %1 did not start. Log the child out, "
                              "then open Steam for them from your own login and sign in again.")
            .arg(title);
    }
    if (said.signIn == SteamSignIn::Now::NoConnection) {
        return QStringLiteral("Steam could not reach the internet, so %1 did not start.")
            .arg(title);
    }
    if (said.lastStep == SteamLaunchLog::Step::WaitingForAnswer && said.offline) {
        if (said.cloudQuestion) {
            return QStringLiteral("Steam could not reach the internet to check the cloud saves "
                                  "for %1, so it did not start. Steam is waiting for an answer "
                                  "about it on a window the kiosk hides.")
                .arg(title);
        }
        return QStringLiteral("Steam could not reach the internet, and is waiting for an answer "
                              "about %1 on a window the kiosk hides.")
            .arg(title);
    }
    if (said.lastStep == SteamLaunchLog::Step::WaitingForAnswer) {
        if (said.logOutFreesHeldGame) {
            return QStringLiteral("Steam is waiting for an answer about %1 on a window the kiosk "
                                  "hides. Log the child out and back in, then try it again.")
                .arg(title);
        }
        return QStringLiteral(
                   "Steam is waiting for an answer about %1 on a window the kiosk hides.")
            .arg(title);
    }
    if (said.lastStep == SteamLaunchLog::Step::Refused) {
        return QStringLiteral("Steam refused to start %1.").arg(title);
    }
    return said.stallNote;
}
