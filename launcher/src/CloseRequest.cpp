// SPDX-License-Identifier: Apache-2.0
#include "CloseRequest.h"

CloseRequest::CloseRequest(QObject* parent) : QObject(parent) {}

CloseRequest::Answer CloseRequest::answer(bool fullscreen, bool terminalOpen, bool logOutAsked,
                                          LogOut::State logOutState,
                                          AppLauncher::State launcherState) {
    if (!fullscreen) {
        return Answer::Close;
    }
    // The grown-up screen is in front of everything else, so it answers first;
    // the coming-soon screen is the same screen and goes back the same way.
    if (launcherState == AppLauncher::State::Failed ||
        launcherState == AppLauncher::State::ComingSoon) {
        return Answer::Dismiss;
    }
    // The launcher never dismisses this screen; only the window closing does.
    if (launcherState == AppLauncher::State::Interrupted) {
        return Answer::Stay;
    }
    // The session is ending under the screen that says so; going back to
    // the tiles now would show them for a moment and then log out anyway.
    if (logOutState == LogOut::State::Leaving) {
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
