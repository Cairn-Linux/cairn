// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include <cstdint>

// Stops the child's Steam and starts it again as the kiosk started it at
// login, for a launch Steam stopped getting ready (SteamStall, #206).
//
// Steam saves its state when it is told to shut down, so it is asked with
// `steam -shutdown`, and then its service, cairn-steam, gets a while to end:
// 4 s on the test VM (2026-10-30). Only that service is ever stopped, the
// one the launcher started at login (SteamAtLogin). Asked to shut down when
// it is not running, Steam starts instead, so a Steam that is not that
// service is left alone, and so is one that will not stop: logging the child
// out ends it for certain.
class SteamRestart : public QObject {
    Q_OBJECT

public:
    explicit SteamRestart(QObject* parent = nullptr);

    // Stops Steam and starts it again. Does nothing while a restart is under
    // way; that one's finished() answers both.
    void start();
    // Tests set this short.
    void setStopLimitMilliseconds(int milliseconds);

    // Asks the child's own systemd whether cairn-steam runs.
    static QStringList checkCommand();
    // Asks the running Steam to save and quit.
    static QStringList stopCommand();

signals:
    // The restart is over: `started` when Steam stopped and was started
    // again.
    void finished(bool started);

private:
    enum class Step : std::uint8_t { Idle, Checking, Stopping, Starting };

    void check();
    // What systemd said about cairn-steam, such as `active`; empty when it
    // could not be asked.
    void onChecked(const QByteArray& state);
    void stop();
    void startSteam();
    void onStarted(bool started);
    void finish(bool started);

    QProcess m_check;
    QProcess m_start;
    QTimer m_nextCheck;
    QElapsedTimer m_sinceStop;
    Step m_step = Step::Idle;
    // How long Steam gets to end after it is asked to: cairn-steam-visit
    // gives a child's Steam the same, after 18 s was seen.
    int m_stopLimitMilliseconds = 25000;
    static constexpr int checkMilliseconds = 1000;
};
