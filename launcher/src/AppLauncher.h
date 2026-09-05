// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QQmlEngine>
#include <QStringList>
#include <QTimer>

#include <cstdint>

// Starts one program at a time for a tile and decides whether the launch went
// well enough to leave the child alone. A program that fails to start, or that
// quits with an error within the settle window, puts the launcher in Failed.
// A window from another program that opens while nothing was launched puts it
// in Interrupted until that window closes. Both show "Something needs a
// grown-up" and nothing else.
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
    QString ownAppId() const;
    void setOwnAppId(const QString& appId);

    // Ignored while a program is starting or running, or while a window that
    // opened on its own is still open. An empty exec fails at once: nothing is
    // set up for that tile yet.
    Q_INVOKABLE void launch(const QString& title, const QStringList& exec);
    // Leaves Failed and goes back to Idle. Interrupted ends only when the
    // window closes, so a child cannot dismiss what a grown-up has to see.
    Q_INVOKABLE void dismiss();

    // What the compositor reports. A window that opens while a program is
    // starting or running belongs to that program and is left alone.
    Q_INVOKABLE void windowOpened(const QString& identifier, const QString& appId,
                                  const QString& title);
    Q_INVOKABLE void windowClosed(const QString& identifier);

signals:
    void stateChanged();
    void titleChanged();
    void settleMillisecondsChanged();
    void ownAppIdChanged();

private:
    void setState(State state);
    void setTitle(const QString& title);
    void onStarted();
    void onErrorOccurred(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);

    QProcess m_process;
    QTimer m_settleTimer;
    State m_state = State::Idle;
    QString m_title;
    QString m_ownAppId;
    // Windows that opened on their own and are still open, by identifier.
    QHash<QString, QString> m_unexpectedWindows;
    bool m_settled = false;
};
