// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <cstdint>

// Whether faillock has locked the account whose password row is open, and
// for how much longer, so the login screen can say so (ADR-0052, #284).
// It reads faillock's settings and its record of wrong tries, and applies
// faillock's own rule (LockRule). PAM stays the gate; this only picks the
// words, and when it cannot be sure it says Unknown, never "locked".
//
// While the account is locked it reads again every second, so the screen
// counts down by the minute and says when a try is allowed again.
class AccountLock : public QObject {
    Q_OBJECT
    QML_ELEMENT
    // The account to watch; empty watches no one.
    Q_PROPERTY(QString loginName READ loginName WRITE setLoginName NOTIFY loginNameChanged)
    // faillock's settings file. Tests give another, naming their own folder.
    Q_PROPERTY(QUrl settingsFile READ settingsFile WRITE setSettingsFile NOTIFY settingsFileChanged)
    // A time for tests to hold still; when not set, the system clock.
    Q_PROPERTY(QDateTime fixedTime READ fixedTime WRITE setFixedTime NOTIFY fixedTimeChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY stateChanged)
    // While Locked: whole minutes left, rounded up as faillock rounds them.
    Q_PROPERTY(int minutesLeft READ minutesLeft NOTIFY stateChanged)
    // How long a lock lasts, in whole minutes rounded up.
    Q_PROPERTY(int lockMinutes READ lockMinutes NOTIFY stateChanged)

public:
    // One for each thing the screen says.
    enum class State : std::uint8_t {
        Unknown,              // cannot tell; the screen keeps its plain words
        Open,                 // two or more wrong tries left
        OneTryLeft,           // the next wrong try locks it for lockMinutes
        OneTryLeftNoTime,     // the same, for a time not said in minutes
        Locked,               // locked for minutesLeft more, two or more
        LockedUnderAMinute,   // locked for a minute or less
        LockedUntilReset,     // locked until someone resets it
        LockedForUnknownTime, // locked, but the clock and the record disagree
        OpenAgain,            // was locked while watched, and is not now
    };
    Q_ENUM(State)

    explicit AccountLock(QObject* parent = nullptr);

    QString loginName() const;
    void setLoginName(const QString& loginName);
    QUrl settingsFile() const;
    void setSettingsFile(const QUrl& file);
    QDateTime fixedTime() const;
    void setFixedTime(const QDateTime& time);

    State state() const;
    bool locked() const;
    int minutesLeft() const;
    int lockMinutes() const;

    // Reads the record again now.
    Q_INVOKABLE void refresh();
    // The screen calls this as it sends a password: it reads the record
    // and keeps whether the account was locked at that moment.
    Q_INVOKABLE void noteTry();
    // Whether the account was locked when the last password was sent. A
    // lock can run out while PAM is still answering, and that try was
    // refused for the lock, not for the password.
    bool lockedAtLastTry() const;

signals:
    void loginNameChanged();
    void settingsFileChanged();
    void fixedTimeChanged();
    void stateChanged();

private:
    void workOut();
    void show(State state, int minutesLeft);

    QString m_loginName;
    QUrl m_settingsFile = QUrl::fromLocalFile(QStringLiteral("/etc/security/faillock.conf"));
    QDateTime m_fixedTime;
    State m_state = State::Unknown;
    int m_minutesLeft = 0;
    int m_lockMinutes = 0;
    bool m_wasLocked = false;
    bool m_lockedAtLastTry = false;
    bool m_busy = false;
    QTimer m_timer;
};
