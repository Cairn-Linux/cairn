// SPDX-License-Identifier: Apache-2.0
#include "ForeignToplevel.h"

ForeignToplevel::ForeignToplevel(struct ::ext_foreign_toplevel_handle_v1* handle, QObject* parent)
    : QObject(parent), QtWayland::ext_foreign_toplevel_handle_v1(handle) {}

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

void ForeignToplevel::ext_foreign_toplevel_handle_v1_closed() {
    emit closed();
}

void ForeignToplevel::ext_foreign_toplevel_handle_v1_done() {
    // The compositor sends "done" again after any later title change; the
    // window only opens once.
    if (m_ready) {
        return;
    }
    m_ready = true;
    emit ready();
}

void ForeignToplevel::ext_foreign_toplevel_handle_v1_title(const QString& title) {
    m_title = title;
}

void ForeignToplevel::ext_foreign_toplevel_handle_v1_app_id(const QString& appId) {
    m_appId = appId;
}

void ForeignToplevel::ext_foreign_toplevel_handle_v1_identifier(const QString& identifier) {
    m_identifier = identifier;
}
