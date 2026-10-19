// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

#include <cstdint>

// Ends the child's session when they choose Log out (ADR-0021), so the next
// person in the family can pick their name at the login screen.
//
// The launcher only knows which program to run. What logging out means,
// letting Steam finish and then ending every process the child has, is the
// session's business (session/bin/cairn-log-out). The kiosk wrapper names
// that program with --log-out; a launcher started without it, such as a
// grown-up's run under Plasma, has no Log out at all.
//
// The program can take seconds, giving Steam its moment, and the launcher
// never hears how it ends: it runs on its own, and when it works the
// launcher ends with the session. So once it has started the launcher is
// Leaving, and a second press starts nothing more; on the laptop
// (2026-10-18) a child who saw no answer pressed Log out six times in two
// seconds. If the session is still here when the wait runs out, the program
// did not end it, and the launcher is DidNotFinish: the child can choose
// again.
class LogOut : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString program READ program WRITE setProgram NOTIFY programChanged)
    Q_PROPERTY(bool available READ available NOTIFY programChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    // How long the session has to end after the program starts. Tests set it
    // short.
    Q_PROPERTY(int waitMilliseconds READ waitMilliseconds WRITE setWaitMilliseconds NOTIFY
                   waitMillisecondsChanged)

public:
    enum class State : std::uint8_t { Idle, Leaving, DidNotFinish };
    Q_ENUM(State)

    explicit LogOut(QObject* parent = nullptr);

    QString program() const;
    void setProgram(const QString& program);
    bool available() const;
    State state() const;
    int waitMilliseconds() const;
    void setWaitMilliseconds(int milliseconds);

    // Starts the program on its own, so it outlives the launcher it ends.
    // False if there is no program or it could not be started. While Leaving
    // it starts nothing and is true: the log-out is already under way.
    Q_INVOKABLE bool start();
    // Leaves DidNotFinish and goes back to Idle, so the next question is
    // asked afresh. Ignored while Leaving: the session is ending.
    Q_INVOKABLE void dismiss();

signals:
    void programChanged();
    void stateChanged();
    void waitMillisecondsChanged();

private:
    void setState(State state);

    QString m_program;
    QTimer m_waitTimer;
    State m_state = State::Idle;
};
