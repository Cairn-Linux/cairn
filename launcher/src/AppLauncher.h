// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "LaunchWindows.h"

#include <QObject>
#include <QProcess>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

#include <cstdint>

// Starts one program at a time for a tile and decides whether the launch went
// well enough to leave the child alone.
//
// The program a child runs is not always the process the launcher starts:
// `steam -applaunch` hands the game to the Steam client and exits within a
// second. So the launcher watches the app's windows, and LaunchWindows decides
// which those are (ADR-0026): while one is on the screen the launcher is
// Running, and when the last leaves the screen it returns to Idle (issue #42).
// A launch that produces no window before the grace timer ends returns to Idle
// quietly; a program that fails to start, or exits with an error before any
// window and before the settle window, is a Failed launch. A window on the
// screen that opened on its own is an Interruption until it closes. Failed and
// Interrupted both show "Something needs a grown-up". A tile with nothing set
// up yet is ComingSoon: the screen says so, with a Back, and needs no
// grown-up. When the program the launcher started exits after the settle
// window, the app is over, and any of its windows still up count as opened on
// their own. The grown-up's give-up key always brings the tiles back.
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
    // Windows with this app id are the launcher's own and never count. The one
    // place an app id decides anything; a program that copied it would not
    // interrupt.
    Q_PROPERTY(QString ownAppId READ ownAppId WRITE setOwnAppId NOTIFY ownAppIdChanged)
    // Start each program in a systemd user scope of its own (AppScope), so the
    // grown-up's give-up key can end it. The kiosk session turns this on.
    Q_PROPERTY(bool scoped MEMBER m_scoped NOTIFY scopedChanged)

public:
    enum class State : std::uint8_t { Idle, Starting, Running, Failed, Interrupted, ComingSoon };
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
    // (Running), or a window that opened on its own is up (Interrupted). An
    // empty exec is ComingSoon at once: nothing is set up for that tile yet.
    Q_INVOKABLE void launch(const QString& title, const QStringList& exec);
    // Leaves Failed or ComingSoon and goes back to Idle. Interrupted ends only
    // when the window closes or a grown-up gives up.
    Q_INVOKABLE void dismiss();
    // The grown-up's give-up key (ADR-0018, ADR-0026): forget the launch and
    // every window open now, and go back to the tiles.
    Q_INVOKABLE void giveUp();

    // What the compositor reports. Whether a window is hidden and which window
    // it belongs to come from the compositor; the app id and title only name
    // it.
    Q_INVOKABLE void windowOpened(const QString& identifier, const QString& appId,
                                  const QString& title, bool hidden = false,
                                  const QString& belongsTo = QString());
    Q_INVOKABLE void windowChanged(const QString& identifier, bool hidden,
                                   const QString& belongsTo = QString());
    Q_INVOKABLE void windowClosed(const QString& identifier);

signals:
    void stateChanged();
    void titleChanged();
    void settleMillisecondsChanged();
    void launchGraceMillisecondsChanged();
    void ownAppIdChanged();
    void scopedChanged();

private:
    void setState(State state);
    void setTitle(const QString& title);
    // Works out the state from the windows on the screen and the launch.
    void update();
    void endLaunch();
    void fail(const QString& reason);
    void onErrorOccurred(QProcess* process, QProcess::ProcessError error);
    void onFinished(QProcess* process, int exitCode, QProcess::ExitStatus exitStatus);
    void onLaunchGraceTimeout();

    // The program started for the current launch, or nullptr once the launch
    // is over. One from an earlier launch may still be running, a game that
    // hid its window or the Steam client it started; it runs on until it exits.
    QProcess* m_process = nullptr;
    QTimer m_settleTimer;
    QTimer m_launchGraceTimer;
    QTimer m_exitGraceTimer;
    LaunchWindows m_windows;
    State m_state = State::Idle;
    QString m_title;
    QString m_launchTitle;
    QString m_ownAppId;
    bool m_settled = false;
    bool m_scoped = false;
    int m_launches = 0;
};
