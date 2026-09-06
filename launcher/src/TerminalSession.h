// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Interpreter.h"
#include "OutputModel.h"
#include "TileModel.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

// One sitting at the restricted shell, inside the launcher window. Builds
// the shell's world from the tiles (the doors) and the built-in home for
// this child, feeds typed lines to the interpreter, keeps the output and the
// history, and asks the launcher to start what open names. It starts
// nothing itself.
class TerminalSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(TileModel* tiles READ tiles WRITE setTiles NOTIFY tilesChanged)
    // Names the child's folder in home. Defaults to the login's home folder.
    Q_PROPERTY(QString childName READ childName WRITE setChildName NOTIFY childNameChanged)
    Q_PROPERTY(QString location READ location NOTIFY locationChanged)
    Q_PROPERTY(OutputModel* output READ output CONSTANT)

public:
    explicit TerminalSession(QObject* parent = nullptr);

    TileModel* tiles() const;
    void setTiles(TileModel* tiles);
    QString childName() const;
    void setChildName(const QString& name);
    QString location() const;
    OutputModel* output() const;

    // A fresh sitting: empty screen, at the root, world rebuilt from the tiles.
    Q_INVOKABLE void reset();
    Q_INVOKABLE void run(const QString& line);
    // The rest of the most likely word for what is typed so far, or nothing.
    Q_INVOKABLE QString ghost(const QString& line) const;
    // Lines typed earlier this sitting; 1 is the latest. Empty past the end.
    Q_INVOKABLE QString recall(int stepsBack) const;

signals:
    void tilesChanged();
    void childNameChanged();
    void locationChanged();
    void launchRequested(const QString& title, const QStringList& exec);
    void left();

private:
    World worldFromTiles() const;

    TileModel* m_tiles = nullptr;
    QString m_childName;
    Interpreter m_interpreter;
    OutputModel* m_output;
    QStringList m_history;
};
