// SPDX-License-Identifier: Apache-2.0
#include "SteamRestart.h"

#include "SteamAtLogin.h"

#include <QDebug>

SteamRestart::SteamRestart(QObject* parent)
    : QObject(parent), m_check(this), m_start(this), m_nextCheck(this) {
    m_nextCheck.setSingleShot(true);
    m_nextCheck.setInterval(checkMilliseconds);
    connect(&m_nextCheck, &QTimer::timeout, this, &SteamRestart::check);
    connect(&m_check, &QProcess::finished, this,
            [this] { onChecked(m_check.readAllStandardOutput().trimmed()); });
    connect(&m_check, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            onChecked({});
        }
    });
    // systemd-run says why it could not start the service; that goes to the
    // journal with the launcher's own lines.
    m_start.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(&m_start, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                onStarted(exitStatus == QProcess::NormalExit && exitCode == 0);
            });
    connect(&m_start, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            onStarted(false);
        }
    });
}

void SteamRestart::start() {
    if (m_step != Step::Idle) {
        return;
    }
    m_step = Step::Checking;
    check();
}

void SteamRestart::setStopLimitMilliseconds(int milliseconds) {
    m_stopLimitMilliseconds = milliseconds;
}

QStringList SteamRestart::checkCommand() {
    return {QStringLiteral("systemctl"), QStringLiteral("--user"), QStringLiteral("is-active"),
            SteamAtLogin::unit()};
}

QStringList SteamRestart::stopCommand() {
    return {QStringLiteral("steam"), QStringLiteral("-shutdown")};
}

void SteamRestart::check() {
    const QStringList command = checkCommand();
    m_check.start(command.first(), command.mid(1));
}

void SteamRestart::onChecked(const QByteArray& state) {
    if (m_step == Step::Checking) {
        if (state != "active") {
            qWarning().noquote() << QStringLiteral("Steam is not running as the launcher started "
                                                   "it, so it was not started again.");
            finish(false);
            return;
        }
        stop();
        return;
    }
    if (m_step != Step::Stopping) {
        return;
    }
    // A service started with --collect is gone once it is inactive or has
    // failed, every process in it with it, and its name is free again.
    if (state == "inactive" || state == "failed") {
        startSteam();
        return;
    }
    if (m_sinceStop.elapsed() >= m_stopLimitMilliseconds) {
        qWarning().noquote() << QStringLiteral(
            "Steam did not stop when asked, so it was not started again.");
        finish(false);
        return;
    }
    m_nextCheck.start();
}

void SteamRestart::stop() {
    const QStringList command = stopCommand();
    QProcess stopping;
    stopping.setProgram(command.first());
    stopping.setArguments(command.mid(1));
    // Steam's own output goes nowhere; the journal says what came of it.
    stopping.setStandardOutputFile(QProcess::nullDevice());
    stopping.setStandardErrorFile(QProcess::nullDevice());
    if (!stopping.startDetached()) {
        qWarning().noquote() << QStringLiteral(
            "Steam did not stop when asked, so it was not started again.");
        finish(false);
        return;
    }
    m_step = Step::Stopping;
    m_sinceStop.start();
    m_nextCheck.start();
}

void SteamRestart::startSteam() {
    m_step = Step::Starting;
    const QStringList command = SteamAtLogin::command();
    m_start.start(command.first(), command.mid(1));
}

void SteamRestart::onStarted(bool started) {
    if (m_step != Step::Starting) {
        return;
    }
    if (started) {
        qInfo().noquote() << QStringLiteral("Steam stopped and was started again.");
    } else {
        qWarning().noquote() << QStringLiteral("Steam could not be started again.");
    }
    finish(started);
}

void SteamRestart::finish(bool started) {
    m_step = Step::Idle;
    m_nextCheck.stop();
    emit finished(started);
}
