// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "qwayland-wlr-foreign-toplevel-management-unstable-v1.h"

#include <QObject>
#include <QString>
#include <QtWaylandClient/QWaylandClientExtension>

// The compositor's list of every window on the screen, through the
// wlr-foreign-toplevel-management-unstable-v1 protocol (ADR-0026). Only for
// Wayland sessions; the owner checks the platform before creating one. Windows
// are reported as they appear, including any that were already open when the
// launcher started. The template finds the global in the registry and binds
// it; the generated class receives its events. Qt's Wayland bindings are used
// this way, so the multiple-inheritance lint is off.
class ForeignToplevelList // NOLINT(misc-multiple-inheritance)
    : public QWaylandClientExtensionTemplate<ForeignToplevelList>,
      public QtWayland::zwlr_foreign_toplevel_manager_v1 {
    Q_OBJECT

public:
    explicit ForeignToplevelList(QObject* parent);

signals:
    void windowOpened(const QString& identifier, const QString& appId, const QString& title,
                      bool hidden, const QString& belongsTo);
    void windowChanged(const QString& identifier, bool hidden, const QString& belongsTo);
    void windowClosed(const QString& identifier);

protected:
    void zwlr_foreign_toplevel_manager_v1_toplevel(
        struct ::zwlr_foreign_toplevel_handle_v1* toplevel) override;
    void zwlr_foreign_toplevel_manager_v1_finished() override;

private:
    int m_windowsSeen = 0;
};
