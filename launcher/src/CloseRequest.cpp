// SPDX-License-Identifier: Apache-2.0
#include "CloseRequest.h"

CloseRequest::CloseRequest(QObject* parent) : QObject(parent) {}

CloseRequest::Answer CloseRequest::answer(bool fullscreen, bool terminalOpen, bool logOutAsked,
                                          AppLauncher::State launcherState) {
    if (!fullscreen) {
        return Answer::Close;
    }
    // The grown-up screen is in front of everything else, so it answers first.
    if (launcherState == AppLauncher::State::Failed) {
        return Answer::Dismiss;
    }
    // The launcher never dismisses this screen; only the window closing does.
    if (launcherState == AppLauncher::State::Interrupted) {
        return Answer::Stay;
    }
    if (logOutAsked) {
        return Answer::StayLoggedIn;
    }
    if (terminalOpen) {
        return Answer::LeaveTerminal;
    }
    return Answer::Stay;
}
