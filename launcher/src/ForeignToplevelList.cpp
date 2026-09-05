// SPDX-License-Identifier: Apache-2.0
#include "ForeignToplevelList.h"

#include "ForeignToplevel.h"

#include <QDebug>

namespace {
constexpr int protocolVersion = 1;
}

ForeignToplevelList::ForeignToplevelList(QObject* parent)
    : QWaylandClientExtensionTemplate(protocolVersion) {
    setParent(parent);
}

void ForeignToplevelList::ext_foreign_toplevel_list_v1_toplevel(
    struct ::ext_foreign_toplevel_handle_v1* toplevel) {
    auto* window = new ForeignToplevel(toplevel, this);
    connect(window, &ForeignToplevel::ready, this, [this, window] {
        emit windowOpened(window->identifier(), window->appId(), window->title());
    });
    connect(window, &ForeignToplevel::closed, this, [this, window] {
        emit windowClosed(window->identifier());
        window->deleteLater();
    });
}

void ForeignToplevelList::ext_foreign_toplevel_list_v1_finished() {
    qWarning().noquote() << QStringLiteral(
        "The compositor stopped reporting windows; none will be noticed now.");
}
