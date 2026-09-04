// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QProcess>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

#include <cstdint>

// Starts one program at a time for a tile and decides whether the launch went
// well enough to leave the child alone. A program that fails to start, or that
// quits with an error within the settle window, puts the launcher in Failed;
// the screen then says "Something needs a grown-up" and nothing else.
class AppLauncher : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    // The title of the tile that was launched last, for the failure screen.
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    // How long a program must stay alive before an exit is no longer a failed
    // launch. Tests set it short.
    Q_PROPERTY(int settleMilliseconds READ settleMilliseconds WRITE setSettleMilliseconds NOTIFY
                   settleMillisecondsChanged)

public:
    enum class State : std::uint8_t { Idle, Starting, Running, Failed };
    Q_ENUM(State)

    explicit AppLauncher(QObject* parent = nullptr);

    State state() const;
    QString title() const;
    int settleMilliseconds() const;
    void setSettleMilliseconds(int milliseconds);

    // Ignored while a program is starting or running. An empty exec fails at
    // once: nothing is set up for that tile yet.
    Q_INVOKABLE void launch(const QString& title, const QStringList& exec);
    // Leaves Failed and goes back to Idle.
    Q_INVOKABLE void dismiss();

signals:
    void stateChanged();
    void titleChanged();
    void settleMillisecondsChanged();

private:
    void setState(State state);
    void onStarted();
    void onErrorOccurred(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

    QProcess m_process;
    QTimer m_settleTimer;
    State m_state = State::Idle;
    QString m_title;
    bool m_settled = false;
};
