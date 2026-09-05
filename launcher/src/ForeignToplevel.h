// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "qwayland-ext-foreign-toplevel-list-v1.h"

#include <QObject>
#include <QString>

// One window the compositor told us about, ours or another program's. The
// compositor sends its app id and title, then "done"; after that it may send
// "closed". This class turns those protocol events into two plain signals.
// A QObject for the signals and the generated protocol class for the events:
// this is how Qt's Wayland bindings are meant to be used, so the lint is off.
class ForeignToplevel : public QObject, // NOLINT(misc-multiple-inheritance)
                        public QtWayland::ext_foreign_toplevel_handle_v1 {
    Q_OBJECT

public:
    ForeignToplevel(struct ::ext_foreign_toplevel_handle_v1* handle, QObject* parent);
    ~ForeignToplevel() override;

    QString identifier() const;
    QString appId() const;
    QString title() const;

signals:
    // Sent once, when the compositor has finished describing the window.
    void ready();
    // Sent once, when the window has gone.
    void closed();

protected:
    void ext_foreign_toplevel_handle_v1_closed() override;
    void ext_foreign_toplevel_handle_v1_done() override;
    void ext_foreign_toplevel_handle_v1_title(const QString& title) override;
    void ext_foreign_toplevel_handle_v1_app_id(const QString& appId) override;
    void ext_foreign_toplevel_handle_v1_identifier(const QString& identifier) override;

private:
    QString m_identifier;
    QString m_appId;
    QString m_title;
    bool m_ready = false;
};
