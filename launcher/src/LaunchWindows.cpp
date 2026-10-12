// SPDX-License-Identifier: Apache-2.0
#include "LaunchWindows.h"

#include <algorithm>

void LaunchWindows::startLaunch() {
    m_launching = true;
    m_opening = true;
    m_appWasOnScreen = false;
}

void LaunchWindows::appIsOpen() {
    m_opening = false;
}

void LaunchWindows::endLaunch() {
    m_launching = false;
    m_opening = false;
    for (Window& window : m_windows) {
        if (window.owner == Owner::App) {
            window.owner = window.hidden ? Owner::Undecided : Owner::OnItsOwn;
        }
    }
}

void LaunchWindows::forgetAll() {
    m_launching = false;
    m_opening = false;
    m_appWasOnScreen = false;
    for (Window& window : m_windows) {
        window.owner = Owner::Forgotten;
    }
}

void LaunchWindows::opened(const QString& identifier, const QString& name, bool hidden,
                           const QString& belongsTo) {
    Window window{
        .name = name, .hidden = hidden, .belongsTo = belongsTo, .owner = Owner::Undecided};
    if (!hidden) {
        decide(window);
    }
    m_windows.insert(identifier, window);
    m_order.append(identifier);
}

void LaunchWindows::changed(const QString& identifier, bool hidden, const QString& belongsTo) {
    const auto found = m_windows.find(identifier);
    if (found == m_windows.end()) {
        return;
    }
    found->belongsTo = belongsTo;
    found->hidden = hidden;
    if (!hidden && found->owner == Owner::Undecided) {
        decide(*found);
    }
}

void LaunchWindows::closed(const QString& identifier) {
    m_windows.remove(identifier);
    m_order.removeOne(identifier);
}

bool LaunchWindows::hidden(const QString& identifier) const {
    const auto found = m_windows.constFind(identifier);
    return found != m_windows.constEnd() && found->hidden;
}

bool LaunchWindows::launching() const {
    return m_launching;
}

bool LaunchWindows::opening() const {
    return m_opening;
}

bool LaunchWindows::appOnScreen() const {
    return onScreen(Owner::App);
}

bool LaunchWindows::appWasOnScreen() const {
    return m_appWasOnScreen;
}

bool LaunchWindows::unexpectedOnScreen() const {
    return onScreen(Owner::OnItsOwn);
}

QString LaunchWindows::unexpectedName() const {
    for (const QString& identifier : m_order) {
        const auto window = m_windows.constFind(identifier);
        if (window->owner == Owner::OnItsOwn && !window->hidden) {
            return window->name;
        }
    }
    return {};
}

void LaunchWindows::decide(Window& window) {
    const auto parent = m_windows.constFind(window.belongsTo);
    const bool belongsToTheApp = parent != m_windows.constEnd() && parent->owner == Owner::App;
    if (m_launching && (m_opening || !appOnScreen() || belongsToTheApp)) {
        window.owner = Owner::App;
        m_appWasOnScreen = true;
        return;
    }
    window.owner = Owner::OnItsOwn;
}

bool LaunchWindows::onScreen(Owner owner) const {
    return std::ranges::any_of(m_windows, [owner](const Window& window) {
        return window.owner == owner && !window.hidden;
    });
}
