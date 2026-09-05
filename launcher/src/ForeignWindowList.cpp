// SPDX-License-Identifier: Apache-2.0
#include "ForeignWindowList.h"

#include "ForeignToplevelList.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QTimer>

namespace {
// The app id Qt gives every window it opens: the desktop file name when one
// is set, otherwise the executable's base name. Kept in step with Qt.
QString appIdOfThisProgram() {
    if (!QGuiApplication::desktopFileName().isEmpty()) {
        return QGuiApplication::desktopFileName();
    }
    return QFileInfo(QCoreApplication::applicationFilePath()).baseName();
}
} // namespace

ForeignWindowList::ForeignWindowList(QObject* parent)
    : QObject(parent), m_ownAppId(appIdOfThisProgram()) {
    if (QGuiApplication::platformName() != QStringLiteral("wayland")) {
        qInfo().noquote() << QStringLiteral(
            "Not a Wayland session, so windows from other programs will not be noticed.");
        return;
    }
    m_list = new ForeignToplevelList(this);
    connect(m_list, &ForeignToplevelList::activeChanged, this, &ForeignWindowList::onActiveChanged);
    connect(m_list, &ForeignToplevelList::windowOpened, this, &ForeignWindowList::windowOpened);
    connect(m_list, &ForeignToplevelList::windowClosed, this, &ForeignWindowList::windowClosed);
    // The compositor answers the registry before this fires, so a list that is
    // still unavailable here is one the compositor does not offer.
    QTimer::singleShot(0, this, [this] {
        if (!m_available) {
            qInfo().noquote() << QStringLiteral(
                "This compositor does not report windows, so ones from other programs will "
                "not be noticed.");
        }
    });
}

bool ForeignWindowList::available() const {
    return m_available;
}

QString ForeignWindowList::ownAppId() const {
    return m_ownAppId;
}

void ForeignWindowList::onActiveChanged() {
    const bool active = m_list->isActive();
    if (active == m_available) {
        return;
    }
    m_available = active;
    emit availableChanged();
}
