// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QQmlEngine>
#include <QString>

// Hears the grown-up's give-up key (Ctrl-Alt-Home, ADR-0018). After it ends
// whatever the child launched, session/bin/cairn-give-up writes to a file in
// the session's runtime directory; this watches that file, so the launcher can
// go back to the tiles even when a window it was waiting on never closes
// (ADR-0026).
class GiveUpWatch : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // The file cairn-give-up writes. Set to watch it; tests use their own.
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)

public:
    explicit GiveUpWatch(QObject* parent = nullptr);

    QString path() const;
    void setPath(const QString& path);
    // $XDG_RUNTIME_DIR/cairn/give-up, where cairn-give-up writes in a session.
    static QString sessionPath();

signals:
    void pathChanged();
    // The grown-up pressed the key and the helper has done its work.
    void pressed();

private:
    QFileSystemWatcher m_watcher;
    QString m_path;
};
