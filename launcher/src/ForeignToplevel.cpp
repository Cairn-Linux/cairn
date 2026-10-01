// SPDX-License-Identifier: Apache-2.0
#include "ForeignToplevel.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>

ForeignToplevel::ForeignToplevel(struct ::zwlr_foreign_toplevel_handle_v1* handle,
                                 QString identifier, QObject* parent)
    : QObject(parent), QtWayland::zwlr_foreign_toplevel_handle_v1(handle),
      m_identifier(std::move(identifier)) {}

ForeignToplevel::~ForeignToplevel() {
    // Tell the compositor we are done with the handle. Nothing is sent after
    // "closed", so this is right whether or not the window is still open.
    destroy();
}

QString ForeignToplevel::identifier() const {
    return m_identifier;
}

QString ForeignToplevel::appId() const {
    return m_appId;
}

QString ForeignToplevel::title() const {
    return m_title;
}

bool ForeignToplevel::hidden() const {
    return m_hidden;
}

QString ForeignToplevel::belongsTo() const {
    return m_belongsTo;
}

void ForeignToplevel::zwlr_foreign_toplevel_handle_v1_title(const QString& title) {
    m_title = title;
}

void ForeignToplevel::zwlr_foreign_toplevel_handle_v1_app_id(const QString& appId) {
    m_appId = appId;
}

void ForeignToplevel::zwlr_foreign_toplevel_handle_v1_state(wl_array* state) {
    // The states that hold now, as a list of numbers; minimised is hidden.
    const std::span<const std::uint32_t> states(static_cast<const std::uint32_t*>(state->data),
                                                state->size / sizeof(std::uint32_t));
    m_hidden =
        std::ranges::find(states, static_cast<std::uint32_t>(state_minimized)) != states.end();
}

void ForeignToplevel::zwlr_foreign_toplevel_handle_v1_parent(
    struct ::zwlr_foreign_toplevel_handle_v1* parent) {
    // Every handle comes from our list, so a parent is one of ours. Its name is
    // kept rather than the object, which may go before this window does.
    auto* const window =
        parent == nullptr ? nullptr : static_cast<ForeignToplevel*>(fromObject(parent));
    m_belongsTo = window == nullptr ? QString() : window->identifier();
}

void ForeignToplevel::zwlr_foreign_toplevel_handle_v1_done() {
    if (!m_ready) {
        m_ready = true;
        emit ready();
        return;
    }
    emit changed();
}

void ForeignToplevel::zwlr_foreign_toplevel_handle_v1_closed() {
    emit closed();
}
