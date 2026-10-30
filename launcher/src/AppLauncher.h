// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "LaunchWindows.h"
#include "SteamGameWait.h"
#include "SteamStartup.h"

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
// which those are (ADR-0026). The launcher stays Starting until one of them
// has stayed on the screen for the steady wait, so a splash that comes and
// goes before the game's own window is part of the start (ADR-0028). Then it
// is Running, and when the app's last window leaves the screen it returns to
// Idle (issue #42). A Steam game Steam is still working on is waited for past
// the grace, up to a limit (SteamGameWait, ADR-0029); while Steam's log says
// it is working on the game, the starting screen says it is getting ready
// (ADR-0036). A tap Steam lost while it updated itself is sent again once the
// new client has started (ADR-0037). A Steam that cannot sign in ends the
// wait sooner, and the journal says why (#182). While Steam updates itself,
// its updater's windows are neither the app's nor on their own, and the
// launcher asks for each to be hidden, so its Cancel button is out of the
// child's reach (#183). A launch that has put
// nothing on the screen when the grace timer ends, or when its program exits
// after the settle window, is Failed (ADR-0030), and its window counts as the
// app's if it comes late; one whose window came and went returns to Idle. A
// program that fails to start, or exits with an error before any window and
// before the settle window, is Failed too. A window on the screen that opened
// on its own is an Interruption until it closes. Failed and Interrupted both
// show "Something needs a grown-up". A tile with nothing set up yet is
// ComingSoon: the screen says so, with a Back, and needs no grown-up. When the
// program the launcher started exits after the settle window, the app is
// over, and any of its windows still up count as opened on their own. The
// grown-up's give-up key always brings the tiles back.
class AppLauncher : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    // True in Failed and Interrupted: the screen shows the grown-up message.
    Q_PROPERTY(bool needsGrownUp READ needsGrownUp NOTIFY stateChanged)
    // The tile that was launched last, or the window that opened on its own,
    // for the one sentence on the grown-up screen that names it.
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    // While Starting: a grace has ended and Steam's log said it was still
    // working on the game, so the starting screen says it is getting ready
    // (ADR-0036). False in every other state.
    Q_PROPERTY(bool gettingReady READ gettingReady NOTIFY gettingReadyChanged)
    // How long a program must stay alive before an exit is no longer a failed
    // launch. Tests set it short.
    Q_PROPERTY(int settleMilliseconds READ settleMilliseconds WRITE setSettleMilliseconds NOTIFY
                   settleMillisecondsChanged)
    // How long to wait for the app's window after a launch before deciding
    // nothing came and returning to the tiles. Tests set it short.
    Q_PROPERTY(int launchGraceMilliseconds READ launchGraceMilliseconds WRITE
                   setLaunchGraceMilliseconds NOTIFY launchGraceMillisecondsChanged)
    // How long the app's window must stay on the screen before the app counts
    // as open and the starting screen goes. Zero means at once. Tests set it.
    Q_PROPERTY(int steadyMilliseconds READ steadyMilliseconds WRITE setSteadyMilliseconds NOTIFY
                   steadyMillisecondsChanged)
    // Windows with this app id are the launcher's own and never count. The one
    // place an app id decides anything; a program that copied it would not
    // interrupt.
    Q_PROPERTY(QString ownAppId READ ownAppId WRITE setOwnAppId NOTIFY ownAppIdChanged)
    // Start each program in a systemd user scope of its own (AppScope), so the
    // grown-up's give-up key can end it. The kiosk session turns this on.
    Q_PROPERTY(bool scoped MEMBER m_scoped NOTIFY scopedChanged)
    // The kiosk started the Steam client at login (ADR-0027). Until a minute
    // after that, a Steam game with no window when the grace ends gets another
    // grace while Steam starts (ADR-0029). Set once, when the launcher starts.
    Q_PROPERTY(bool steamStartedAtLogin READ steamStartedAtLogin WRITE setSteamStartedAtLogin NOTIFY
                   steamStartedAtLoginChanged)

public:
    enum class State : std::uint8_t { Idle, Starting, Running, Failed, Interrupted, ComingSoon };
    Q_ENUM(State)

    explicit AppLauncher(QObject* parent = nullptr);

    State state() const;
    bool needsGrownUp() const;
    QString title() const;
    bool gettingReady() const;
    int settleMilliseconds() const;
    void setSettleMilliseconds(int milliseconds);
    int launchGraceMilliseconds() const;
    void setLaunchGraceMilliseconds(int milliseconds);
    int steadyMilliseconds() const;
    void setSteadyMilliseconds(int milliseconds);
    QString ownAppId() const;
    void setOwnAppId(const QString& appId);
    bool steamStartedAtLogin() const;
    void setSteamStartedAtLogin(bool started);
    // Tests set this short: how long a Steam that cannot reach its servers
    // is waited for (SteamGameWait).
    void setSteamOfflineLimitMilliseconds(int milliseconds);

    // Ignored while a launch is in flight (Starting), an app's window is up
    // (Running), or a window that opened on its own is up (Interrupted). An
    // empty exec is ComingSoon at once: nothing is set up for that tile yet.
    Q_INVOKABLE void launch(const QString& title, const QStringList& exec);
    // Leaves Failed or ComingSoon and goes back to Idle. After a launch that put
    // nothing on the screen, its window counts as the app's until then, so a
    // game that comes late takes over from the grown-up screen. Interrupted
    // ends only when the window closes or a grown-up gives up.
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
    void gettingReadyChanged();
    void settleMillisecondsChanged();
    void launchGraceMillisecondsChanged();
    void steadyMillisecondsChanged();
    void ownAppIdChanged();
    void scopedChanged();
    void steamStartedAtLoginChanged();
    // Steam's updater put this window on the screen: the compositor should
    // hide it (#183). Asked again if the window comes back.
    void windowToHide(const QString& identifier);

private:
    void setState(State state);
    void setTitle(const QString& title);
    void setGettingReady(bool gettingReady);
    // Works out the state from the windows on the screen and the launch.
    void update();
    // Tells the windows whether one that comes on the screen now is Steam's
    // updater's (#183).
    void noteSteamUpdating();
    // Asks for a window to be hidden if it is Steam's updater's and shown.
    void hideIfSteamUpdater(const QString& identifier);
    void appIsOpen();
    void endLaunch();
    // Nothing of the app came on the screen in time: Failed, but the launch
    // stays, so a window that comes late is still the app's.
    void nothingCame();
    void fail(const QString& reason);
    void onErrorOccurred(QProcess* process, QProcess::ProcessError error);
    void onFinished(QProcess* process, int exitCode, QProcess::ExitStatus exitStatus);
    void onLaunchGraceTimeout();
    // Runs the tile's program for the current launch, in a scope of its own
    // when scoped.
    void startProgram(const QStringList& exec);

    // The program started for the current launch, or nullptr once the launch
    // is over. One from an earlier launch may still be running, a game that
    // hid its window or the Steam client it started; it runs on until it exits.
    QProcess* m_process = nullptr;
    QTimer m_settleTimer;
    QTimer m_launchGraceTimer;
    QTimer m_exitGraceTimer;
    QTimer m_steadyTimer;
    LaunchWindows m_windows;
    SteamGameWait m_steamGame;
    SteamStartup m_steamStartup;
    State m_state = State::Idle;
    QString m_title;
    QString m_launchTitle;
    QStringList m_launchExec;
    QString m_ownAppId;
    bool m_settled = false;
    bool m_gettingReady = false;
    // The program quit after the app's window came up, before the app was open.
    bool m_quitWhileOpening = false;
    // Nothing came in time and the grown-up screen is up, but the launch
    // stays: a window now is the app arriving late (ADR-0030).
    bool m_waitingLate = false;
    bool m_scoped = false;
    bool m_steamStartedAtLogin = false;
    int m_launches = 0;
};
