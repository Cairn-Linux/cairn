// SPDX-License-Identifier: Apache-2.0
#include "AppLauncher.h"

#include "AppScope.h"

#include <QDebug>

namespace {
constexpr int defaultSettleMilliseconds = 5000;
// Long enough for a Flatpak app or a Steam game to draw its first window on a
// slow machine, short enough that a launch which produces nothing does not
// leave the child staring. A first-time Steam download can outlast it; that
// falls back to the grown-up screen and is refined in P1-13.
constexpr int defaultLaunchGraceMilliseconds = 15000;
// How long an app's windows have to close after its program exits before any
// still up count as opened on their own. Without it the moment between a
// game's process ending and its window going would flash the grown-up screen.
constexpr int exitGraceMilliseconds = 2000;
// How long the app's window must stay up before the app counts as open. A
// Steam or Proton game often shows a window for a moment and replaces it; on
// the laptop (2026-10-08) the starting screen went with that first
// window and the tiles flashed between it and the game's own.
constexpr int defaultSteadyMilliseconds = 3000;
} // namespace

AppLauncher::AppLauncher(QObject* parent)
    : QObject(parent), m_settleTimer(this), m_launchGraceTimer(this), m_exitGraceTimer(this),
      m_steadyTimer(this) {
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(defaultSettleMilliseconds);
    connect(&m_settleTimer, &QTimer::timeout, this, [this] { m_settled = true; });
    m_launchGraceTimer.setSingleShot(true);
    m_launchGraceTimer.setInterval(defaultLaunchGraceMilliseconds);
    connect(&m_launchGraceTimer, &QTimer::timeout, this, &AppLauncher::onLaunchGraceTimeout);
    m_steadyTimer.setSingleShot(true);
    m_steadyTimer.setInterval(defaultSteadyMilliseconds);
    connect(&m_steadyTimer, &QTimer::timeout, this, [this] {
        if (m_windows.opening() && m_windows.appOnScreen()) {
            appIsOpen();
            update();
        }
    });
    m_exitGraceTimer.setSingleShot(true);
    m_exitGraceTimer.setInterval(exitGraceMilliseconds);
    connect(&m_exitGraceTimer, &QTimer::timeout, this, [this] {
        if (m_windows.launching()) {
            endLaunch();
            update();
        }
    });
}

AppLauncher::State AppLauncher::state() const {
    return m_state;
}

bool AppLauncher::needsGrownUp() const {
    return m_state == State::Failed || m_state == State::Interrupted;
}

QString AppLauncher::title() const {
    return m_title;
}

int AppLauncher::settleMilliseconds() const {
    return m_settleTimer.interval();
}

void AppLauncher::setSettleMilliseconds(int milliseconds) {
    if (milliseconds == m_settleTimer.interval()) {
        return;
    }
    m_settleTimer.setInterval(milliseconds);
    emit settleMillisecondsChanged();
}

int AppLauncher::launchGraceMilliseconds() const {
    return m_launchGraceTimer.interval();
}

void AppLauncher::setLaunchGraceMilliseconds(int milliseconds) {
    if (milliseconds == m_launchGraceTimer.interval()) {
        return;
    }
    m_launchGraceTimer.setInterval(milliseconds);
    emit launchGraceMillisecondsChanged();
}

int AppLauncher::steadyMilliseconds() const {
    return m_steadyTimer.interval();
}

void AppLauncher::setSteadyMilliseconds(int milliseconds) {
    if (milliseconds == m_steadyTimer.interval()) {
        return;
    }
    m_steadyTimer.setInterval(milliseconds);
    emit steadyMillisecondsChanged();
}

QString AppLauncher::ownAppId() const {
    return m_ownAppId;
}

void AppLauncher::setOwnAppId(const QString& appId) {
    if (appId == m_ownAppId) {
        return;
    }
    m_ownAppId = appId;
    emit ownAppIdChanged();
}

bool AppLauncher::steamStartedAtLogin() const {
    return m_steamStartedAtLogin;
}

void AppLauncher::setSteamStartedAtLogin(bool started) {
    if (started == m_steamStartedAtLogin) {
        return;
    }
    m_steamStartedAtLogin = started;
    if (started) {
        m_steamGame.steamStartedAtLogin();
    }
    emit steamStartedAtLoginChanged();
}

void AppLauncher::launch(const QString& title, const QStringList& exec) {
    if (m_state == State::Starting || m_state == State::Running || m_state == State::Interrupted) {
        return;
    }
    m_launchTitle = title;
    setTitle(title);
    if (exec.isEmpty()) {
        qWarning().noquote() << QStringLiteral("No program is set up for the tile %1.").arg(title);
        setState(State::ComingSoon);
        return;
    }

    m_settled = false;
    m_quitWhileOpening = false;
    m_windows.startLaunch();
    m_steamGame.startLaunch(exec);
    setState(State::Starting);
    m_settleTimer.start();
    m_launchGraceTimer.start();
    m_exitGraceTimer.stop();
    ++m_launches;
    const QStringList command = m_scoped ? AppScope::command(exec, m_launches) : exec;
    // A process for each launch, so one left from an earlier launch never
    // stops this one starting. Each deletes itself when its program exits.
    auto* process = new QProcess(this);
    // The child's app draws its own window; its terminal output is not for the child.
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) { onErrorOccurred(process, error); });
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                onFinished(process, exitCode, exitStatus);
            });
    m_process = process;
    process->start(command.first(), command.mid(1));
}

void AppLauncher::dismiss() {
    if (m_state == State::Failed || m_state == State::ComingSoon) {
        setState(State::Idle);
    }
}

void AppLauncher::giveUp() {
    endLaunch();
    m_windows.forgetAll();
    setTitle(m_launchTitle);
    setState(State::Idle);
}

void AppLauncher::windowOpened(const QString& identifier, const QString& appId,
                               const QString& title, bool hidden, const QString& belongsTo) {
    if (!m_ownAppId.isEmpty() && appId == m_ownAppId) {
        return;
    }
    QString name = title.isEmpty() ? appId : title;
    if (name.isEmpty()) {
        name = tr("Another program");
    }
    if (!hidden) {
        // Each window that comes up while the app is opening gets the whole
        // steady wait, so a splash's last moment does not open the app.
        m_steadyTimer.stop();
    }
    m_windows.opened(identifier, name, hidden, belongsTo);
    update();
}

void AppLauncher::windowChanged(const QString& identifier, bool hidden, const QString& belongsTo) {
    if (m_windows.hidden(identifier) && !hidden) {
        m_steadyTimer.stop();
    }
    m_windows.changed(identifier, hidden, belongsTo);
    update();
}

void AppLauncher::windowClosed(const QString& identifier) {
    m_windows.closed(identifier);
    update();
}

void AppLauncher::setState(State state) {
    if (state == m_state) {
        return;
    }
    m_state = state;
    emit stateChanged();
}

void AppLauncher::setTitle(const QString& title) {
    if (title == m_title) {
        return;
    }
    m_title = title;
    emit titleChanged();
}

void AppLauncher::update() {
    // While the app is still opening and the grace timer runs, a window that
    // came and went was a splash; keep waiting for the next. Not when the
    // program itself has quit since its window came up: that is the app
    // ending, and nothing more will come.
    const bool stillOpening =
        m_windows.opening() && m_launchGraceTimer.isActive() && !m_quitWhileOpening;
    if (m_windows.launching() && m_windows.appWasOnScreen() && !m_windows.appOnScreen() &&
        !stillOpening) {
        // The app's last window has left the screen, so the app has too.
        endLaunch();
    }
    if (m_windows.unexpectedOnScreen()) {
        if (m_state != State::Interrupted) {
            const QString name = m_windows.unexpectedName();
            qWarning().noquote() << QStringLiteral("%1 opened a window on its own.").arg(name);
            setTitle(name);
            setState(State::Interrupted);
        }
        return;
    }
    if (m_windows.opening() && m_windows.appOnScreen() && m_steadyTimer.interval() <= 0) {
        appIsOpen();
    }
    if (m_windows.opening()) {
        // The app is open once its window has stayed up for the steady wait.
        if (!m_windows.appOnScreen()) {
            m_steadyTimer.stop();
        } else if (!m_steadyTimer.isActive()) {
            m_steadyTimer.start();
        }
        setTitle(m_launchTitle);
        setState(State::Starting);
    } else if (m_windows.appOnScreen()) {
        setTitle(m_launchTitle);
        setState(State::Running);
    } else if (m_windows.launching()) {
        setTitle(m_launchTitle);
        setState(State::Starting);
    } else if (m_state != State::Failed && m_state != State::ComingSoon) {
        setState(State::Idle);
    }
}

void AppLauncher::appIsOpen() {
    m_windows.appIsOpen();
    m_launchGraceTimer.stop();
}

void AppLauncher::endLaunch() {
    m_windows.endLaunch();
    m_quitWhileOpening = false;
    m_steadyTimer.stop();
    m_settleTimer.stop();
    m_launchGraceTimer.stop();
    m_exitGraceTimer.stop();
    // A program still running runs on by itself; its exit no longer matters.
    m_process = nullptr;
}

void AppLauncher::fail(const QString& reason) {
    qWarning().noquote() << reason;
    endLaunch();
    setTitle(m_launchTitle);
    setState(State::Failed);
}

void AppLauncher::onErrorOccurred(QProcess* process, QProcess::ProcessError error) {
    if (error != QProcess::FailedToStart) {
        return;
    }
    // A program that never started never finishes, so this is its last word.
    process->deleteLater();
    if (process != m_process) {
        return;
    }
    fail(QStringLiteral("The program for %1 did not start: %2")
             .arg(m_launchTitle, process->program()));
}

void AppLauncher::onFinished(QProcess* process, int exitCode, QProcess::ExitStatus exitStatus) {
    process->deleteLater();
    if (process != m_process) {
        return;
    }
    if (m_settled) {
        // `steam -applaunch` only hands the game over, so its exit never ends
        // the game; right after login it can return late (ADR-0029).
        if (m_steamGame.steamGame()) {
            return;
        }
        // It ran past the settle window and is gone, so the app is over. Its
        // windows get a moment to close before any left count as unexpected.
        m_exitGraceTimer.start();
        return;
    }
    m_settleTimer.stop();
    // If the app's window is already up, the launching process exiting is
    // expected: `steam -applaunch` returns while the game runs on, and the
    // window closing, not the process, ends the app. A program that quits
    // after its own window came up is the app ending, as when a child closes
    // it at once; `steam -applaunch` hands off before any window.
    if (m_windows.appWasOnScreen()) {
        if (m_windows.opening()) {
            m_quitWhileOpening = true;
            update();
        }
        return;
    }
    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        fail(QStringLiteral("The program for %1 quit right away with code %2.")
                 .arg(m_launchTitle)
                 .arg(exitCode));
        return;
    }
    // A clean exit with no window yet is the fire-and-forget launcher handing
    // off. Keep waiting for the app's window; the grace timer ends the wait.
}

void AppLauncher::onLaunchGraceTimeout() {
    if (!m_windows.launching()) {
        return;
    }
    if (m_steamGame.stillComing()) {
        // Steam is still starting, or has the game in hand: wait another grace
        // before looking again (ADR-0029). Also with a window up, so a splash
        // that closes after this moment is still part of the start.
        m_launchGraceTimer.start();
        return;
    }
    // The app did not open in time and no window of it is up: nothing came,
    // or a window came and went. Nothing is wrong to show a child; go back to
    // the tiles, where they can try again. A window that is up now gets the
    // rest of its steady wait.
    if (!m_windows.appOnScreen()) {
        endLaunch();
        update();
    }
}
