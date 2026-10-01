// SPDX-License-Identifier: Apache-2.0
#include "ForeignToplevelList.h"

#include "ForeignToplevel.h"

#include <QDebug>

namespace {
// Version 3 is the first to say which window another belongs to.
constexpr int protocolVersion = 3;
} // namespace

ForeignToplevelList::ForeignToplevelList(QObject* parent)
    : QWaylandClientExtensionTemplate(protocolVersion) {
    setParent(parent);
}

void ForeignToplevelList::zwlr_foreign_toplevel_manager_v1_toplevel(
    struct ::zwlr_foreign_toplevel_handle_v1* toplevel) {
    ++m_windowsSeen;
    auto* window = new ForeignToplevel(toplevel, QString::number(m_windowsSeen), this);
    connect(window, &ForeignToplevel::ready, this, [this, window] {
        emit windowOpened(window->identifier(), window->appId(), window->title(), window->hidden(),
                          window->belongsTo());
    });
    connect(window, &ForeignToplevel::changed, this, [this, window] {
        emit windowChanged(window->identifier(), window->hidden(), window->belongsTo());
    });
    connect(window, &ForeignToplevel::closed, this, [this, window] {
        emit windowClosed(window->identifier());
        window->deleteLater();
    });
}

void ForeignToplevelList::zwlr_foreign_toplevel_manager_v1_finished() {
    qWarning().noquote() << QStringLiteral(
        "The compositor stopped reporting windows; none will be noticed now.");
}
