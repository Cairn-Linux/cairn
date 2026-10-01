// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

class ForeignToplevelList;

// Tells the launcher when any window opens, closes, is hidden or shown on the
// screen, so it can tell the app it launched from a window that opened on its
// own (ADR-0026). Safe to create on any platform: only a Wayland compositor
// that offers wlr-foreign-toplevel-management-unstable-v1 makes it available;
// elsewhere it stays quiet and says so once in the terminal.
class ForeignWindowList : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    // The app id the compositor reports for this program's own windows.
    Q_PROPERTY(QString ownAppId READ ownAppId CONSTANT)

public:
    explicit ForeignWindowList(QObject* parent = nullptr);

    bool available() const;
    QString ownAppId() const;

signals:
    void availableChanged();
    void windowOpened(const QString& identifier, const QString& appId, const QString& title,
                      bool hidden, const QString& belongsTo);
    void windowChanged(const QString& identifier, bool hidden, const QString& belongsTo);
    void windowClosed(const QString& identifier);

private:
    void onActiveChanged();

    ForeignToplevelList* m_list = nullptr;
    QString m_ownAppId;
    bool m_available = false;
};
