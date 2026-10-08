// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include <cstdint>

// Which windows on the screen are the app a tile launched, and which opened on
// their own (ADR-0026, #99).
//
// The compositor cannot say which program drew a window, but it can say two
// things a program cannot fake: whether the window is hidden, and which window
// it belongs to. So a hidden window never counts: the child cannot see it, so
// it must not keep the tiles waiting or bring up the grown-up screen. While a
// launch is opening, every window on the screen is the app's: a game may show
// a splash or a blank window before its own (ADR-0028). Once the app is open,
// only a window that belongs to one of the app's, such as a dialog, is the
// app's too. Any other window opened on its own. A window's app id or title
// only ever names it.
class LaunchWindows {
public:
    // A tile launched something: every window on the screen is the app's until
    // it is open.
    void startLaunch();
    // The app's window has stayed on the screen long enough: the app is open.
    void appIsOpen();
    // The app is over. Its windows still on the screen opened on their own now;
    // its hidden ones are decided again if they are ever shown.
    void endLaunch();
    // The grown-up's give-up key: forget every window open now, whatever it is.
    void forgetAll();

    void opened(const QString& identifier, const QString& name, bool hidden,
                const QString& belongsTo);
    void changed(const QString& identifier, bool hidden, const QString& belongsTo);
    void closed(const QString& identifier);

    bool launching() const;
    // A launch is under way and the app is not open yet.
    bool opening() const;
    // One of the app's windows is on the screen now.
    bool appOnScreen() const;
    // One of the app's windows has been on the screen since the launch.
    bool appWasOnScreen() const;
    // A window that opened on its own is on the screen now.
    bool unexpectedOnScreen() const;
    // The name of the first such window still on the screen.
    QString unexpectedName() const;

private:
    enum class Owner : std::uint8_t { Undecided, App, OnItsOwn, Forgotten };

    struct Window {
        QString name;
        bool hidden = false;
        QString belongsTo;
        Owner owner = Owner::Undecided;
    };

    // A window is decided when it is first on the screen, never while hidden.
    void decide(Window& window);
    bool onScreen(Owner owner) const;

    QHash<QString, Window> m_windows;
    // The windows in the order they opened, so "first" means something.
    QList<QString> m_order;
    bool m_launching = false;
    bool m_opening = false;
    bool m_appWasOnScreen = false;
};
