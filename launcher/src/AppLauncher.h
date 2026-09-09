// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QQmlEngine>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <cstdint>

// Starts one program at a time for a tile and decides whether the launch went
// well enough to leave the child alone.
//
// The program a child runs is not the process the launcher starts. `flatpak
// run` and `steam -applaunch` hand the real app to a background service and
// exit within a second, so the launcher cannot watch the app by its process:
// it would think the app had closed the moment it opened. Instead the launcher
// watches for the app's window. A window that appears after a launch is the
// app; while it is up the launcher is Running, and it returns to Idle when
// that window closes, whatever the launching process did in between (issue
// #42). A launch that produces no window before the grace timer ends returns
// to Idle quietly; a program that fails to start, or exits with an error
// before any window and before the settle window, is a Failed launch. A window
// that appears while nothing was launched is an Interruption. Failed and
// Interrupted both show "Something needs a grown-up".
class AppLauncher : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    // True in Failed and Interrupted: the screen shows the grown-up message.
    Q_PROPERTY(bool needsGrownUp READ needsGrownUp NOTIFY stateChanged)
    // The tile that was launched last, or the window that opened on its own,
    // for the one sentence on the grown-up screen that names it.
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    // How long a program must stay alive before an exit is no longer a failed
    // launch. Tests set it short.
    Q_PROPERTY(int settleMilliseconds READ settleMilliseconds WRITE setSettleMilliseconds NOTIFY
                   settleMillisecondsChanged)
    // How long to wait for the app's window after a launch before deciding
    // nothing came and returning to the tiles. Tests set it short.
    Q_PROPERTY(int launchGraceMilliseconds READ launchGraceMilliseconds WRITE
                   setLaunchGraceMilliseconds NOTIFY launchGraceMillisecondsChanged)
    // Windows with this app id are the launcher's own and never interrupt.
    // Empty means no window is treated as ours.
    Q_PROPERTY(QString ownAppId READ ownAppId WRITE setOwnAppId NOTIFY ownAppIdChanged)

public:
    enum class State : std::uint8_t { Idle, Starting, Running, Failed, Interrupted };
    Q_ENUM(State)

    explicit AppLauncher(QObject* parent = nullptr);

    State state() const;
    bool needsGrownUp() const;
    QString title() const;
    int settleMilliseconds() const;
    void setSettleMilliseconds(int milliseconds);
    int launchGraceMilliseconds() const;
    void setLaunchGraceMilliseconds(int milliseconds);
    QString ownAppId() const;
    void setOwnAppId(const QString& appId);

    // Ignored while a launch is in flight (Starting), an app's window is up
    // (Running), or a window that opened on its own is still open
    // (Interrupted). An empty exec fails at once: nothing is set up for that
    // tile yet.
    Q_INVOKABLE void launch(const QString& title, const QStringList& exec);
    // Leaves Failed and goes back to Idle. Interrupted ends only when the
    // window closes, so a child cannot dismiss what a grown-up has to see.
    Q_INVOKABLE void dismiss();

    // What the compositor reports. A window that opens while a launch is in
    // flight or an app is running is that app's, and keeps the launcher
    // Running until it closes. A window that opens while nothing was launched
    // is an interruption.
    Q_INVOKABLE void windowOpened(const QString& identifier, const QString& appId,
                                  const QString& title);
    Q_INVOKABLE void windowClosed(const QString& identifier);

signals:
    void stateChanged();
    void titleChanged();
    void settleMillisecondsChanged();
    void launchGraceMillisecondsChanged();
    void ownAppIdChanged();

private:
    void setState(State state);
    void setTitle(const QString& title);
    void onErrorOccurred(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onLaunchGraceTimeout();

    QProcess m_process;
    QTimer m_settleTimer;
    QTimer m_launchGraceTimer;
    State m_state = State::Idle;
    QString m_title;
    QString m_ownAppId;
    // The launched app's windows, by identifier. While any is open the app is
    // on screen and the launcher is Running.
    QSet<QString> m_ownedWindows;
    // Windows that opened on their own and are still open, by identifier.
    QHash<QString, QString> m_unexpectedWindows;
    // True once the launching process has exited. On its own it means nothing:
    // the app may have only just opened its window.
    bool m_processGone = false;
    bool m_settled = false;
};
