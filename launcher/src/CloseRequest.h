// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "AppLauncher.h"

#include <QObject>
#include <QQmlEngine>

#include <cstdint>

// Decides what a request to close the launcher's window means (ADR-0019).
//
// In the kiosk the launcher is the child's whole session: labwc runs it with
// -S and ends when it does (ADR-0012), so closing it would log the child out.
// The child's leave key, Super+Q, asks whichever window is in front to close,
// and when that is the launcher the request arrives here. It then means one
// step back, exactly what Escape does on the screen that is showing: a failed
// launch goes back to where the child was, the Terminal goes back to the
// tiles, and at the tiles, or while a window that opened on its own is up,
// nothing happens. The question before logging out (ADR-0021) goes back to
// the tiles, as its Back does. The kiosk makes the launcher fullscreen and
// nothing else does, so a windowed launcher is a grown-up running it under
// Plasma, and there a close closes it like any other window.
class CloseRequest : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    enum class Answer : std::uint8_t { Close, LeaveTerminal, Dismiss, StayLoggedIn, Stay };
    Q_ENUM(Answer)

    explicit CloseRequest(QObject* parent = nullptr);

    Q_INVOKABLE static Answer answer(bool fullscreen, bool terminalOpen, bool logOutAsked,
                                     AppLauncher::State launcherState);
};
