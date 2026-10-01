// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "qwayland-wlr-foreign-toplevel-management-unstable-v1.h"

#include <QObject>
#include <QString>

// One window the compositor told us about, ours or another program's. The
// compositor sends its app id, title, state and the window it belongs to,
// then "done", and again whenever any of that changes; at the end it sends
// "closed". This class turns those protocol events into plain signals.
// Whether the window is hidden and which window it belongs to come from the
// compositor; the app id and title are whatever the program chose (ADR-0026).
// A QObject for the signals and the generated protocol class for the events:
// this is how Qt's Wayland bindings are meant to be used, so the lint is off.
class ForeignToplevel : public QObject, // NOLINT(misc-multiple-inheritance)
                        public QtWayland::zwlr_foreign_toplevel_handle_v1 {
    Q_OBJECT

public:
    ForeignToplevel(struct ::zwlr_foreign_toplevel_handle_v1* handle, QString identifier,
                    QObject* parent);
    ~ForeignToplevel() override;

    // Ours, not the compositor's: this protocol gives windows no name of their own.
    QString identifier() const;
    QString appId() const;
    QString title() const;
    bool hidden() const;
    // The identifier of the window this one belongs to, such as the app a
    // dialog came from, or empty.
    QString belongsTo() const;

signals:
    // Sent once, when the compositor has finished describing the window.
    void ready();
    // Sent after that whenever the window is hidden, shown or given a parent.
    void changed();
    // Sent once, when the window has gone.
    void closed();

protected:
    void zwlr_foreign_toplevel_handle_v1_title(const QString& title) override;
    void zwlr_foreign_toplevel_handle_v1_app_id(const QString& appId) override;
    void zwlr_foreign_toplevel_handle_v1_state(wl_array* state) override;
    void zwlr_foreign_toplevel_handle_v1_parent(
        struct ::zwlr_foreign_toplevel_handle_v1* parent) override;
    void zwlr_foreign_toplevel_handle_v1_done() override;
    void zwlr_foreign_toplevel_handle_v1_closed() override;

private:
    QString m_identifier;
    QString m_appId;
    QString m_title;
    bool m_hidden = false;
    QString m_belongsTo;
    bool m_ready = false;
};
