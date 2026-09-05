// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "qwayland-ext-foreign-toplevel-list-v1.h"

#include <QObject>
#include <QString>
#include <QtWaylandClient/QWaylandClientExtension>

// The compositor's list of every window on the screen, through the
// ext-foreign-toplevel-list-v1 protocol. Only for Wayland sessions; the owner
// checks the platform before creating one. Windows are reported as they
// appear, including any that were already open when the launcher started.
// The template finds the global in the registry and binds it; the generated
// class receives its events. Qt's Wayland bindings are used this way, so the
// multiple-inheritance lint is off.
class ForeignToplevelList // NOLINT(misc-multiple-inheritance)
    : public QWaylandClientExtensionTemplate<ForeignToplevelList,
                                             &QtWayland::ext_foreign_toplevel_list_v1::destroy>,
      public QtWayland::ext_foreign_toplevel_list_v1 {
    Q_OBJECT

public:
    explicit ForeignToplevelList(QObject* parent);

signals:
    void windowOpened(const QString& identifier, const QString& appId, const QString& title);
    void windowClosed(const QString& identifier);

protected:
    void ext_foreign_toplevel_list_v1_toplevel(
        struct ::ext_foreign_toplevel_handle_v1* toplevel) override;
    void ext_foreign_toplevel_list_v1_finished() override;
};
