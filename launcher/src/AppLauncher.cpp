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
} // namespace

AppLauncher::AppLauncher(QObject* parent)
    : QObject(parent), m_settleTimer(this), m_launchGraceTimer(this), m_exitGraceTimer(this) {
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(defaultSettleMilliseconds);
    connect(&m_settleTimer, &QTimer::timeout, this, [this] { m_settled = true; });
    m_launchGraceTimer.setSingleShot(true);
    m_launchGraceTimer.setInterval(defaultLaunchGraceMilliseconds);
    connect(&m_launchGraceTimer, &QTimer::timeout, this, &AppLauncher::onLaunchGraceTimeout);
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
    m_windows.startLaunch();
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
    m_windows.opened(identifier, name, hidden, belongsTo);
    update();
}

void AppLauncher::windowChanged(const QString& identifier, bool hidden, const QString& belongsTo) {
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
    if (m_windows.launching() && m_windows.appWasOnScreen() && !m_windows.appOnScreen()) {
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
    if (m_windows.appOnScreen()) {
        m_launchGraceTimer.stop();
        setTitle(m_launchTitle);
        setState(State::Running);
    } else if (m_windows.launching()) {
        setTitle(m_launchTitle);
        setState(State::Starting);
    } else if (m_state != State::Failed && m_state != State::ComingSoon) {
        setState(State::Idle);
    }
}

void AppLauncher::endLaunch() {
    m_windows.endLaunch();
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
        // It ran past the settle window and is gone, so the app is over. Its
        // windows get a moment to close before any left count as unexpected.
        m_exitGraceTimer.start();
        return;
    }
    m_settleTimer.stop();
    // If the app's window is already up, the launching process exiting is
    // expected: `steam -applaunch` returns while the game runs on, and the
    // window closing, not the process, ends the app.
    if (m_windows.appWasOnScreen()) {
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
    // The launch produced no window in time. Nothing is wrong to show a child;
    // go back to the tiles, where they can try again.
    if (m_windows.launching() && !m_windows.appWasOnScreen()) {
        endLaunch();
        update();
    }
}
