// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

// Ends the child's session when they choose Log out (ADR-0021), so the next
// person in the family can pick their name at the login screen.
//
// The launcher only knows which program to run. What logging out means,
// letting Steam finish and then ending every process the child has, is the
// session's business (session/bin/cairn-log-out). The kiosk wrapper names
// that program with --log-out; a launcher started without it, such as a
// grown-up's run under Plasma, has no Log out at all.
class LogOut : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString program READ program WRITE setProgram NOTIFY programChanged)
    Q_PROPERTY(bool available READ available NOTIFY programChanged)

public:
    explicit LogOut(QObject* parent = nullptr);

    QString program() const;
    void setProgram(const QString& program);
    bool available() const;

    // Starts the program on its own, so it outlives the launcher it ends.
    // False if there is no program or it could not be started.
    Q_INVOKABLE bool start();

signals:
    void programChanged();

private:
    QString m_program;
};
