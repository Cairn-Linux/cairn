// SPDX-License-Identifier: Apache-2.0
#include "AccountLock.h"

#include "FaillockSettings.h"
#include "LockRule.h"
#include "TallyFile.h"

namespace {

constexpr int secondsInMinute = 60;
constexpr int readEveryMs = 1000;

// Rounded up, as faillock's own message does, so a lock of 600 seconds
// reads "10 minutes" here as it does at sudo. faillock lets a try in only
// the second after the count reaches 0, so at each step the real wait can
// be up to a second longer than said. Only "You can try again now." says
// a try is allowed; it never comes early, but the once-a-second read can
// make it up to a second late.
int wholeMinutes(qint64 seconds) {
    return static_cast<int>((seconds + secondsInMinute - 1) / secondsInMinute);
}

} // namespace

AccountLock::AccountLock(QObject* parent) : QObject(parent), m_timer(this) {
    m_timer.setInterval(readEveryMs);
    connect(&m_timer, &QTimer::timeout, this, &AccountLock::refresh);
}

QString AccountLock::loginName() const {
    return m_loginName;
}

void AccountLock::setLoginName(const QString& loginName) {
    if (loginName == m_loginName) {
        return;
    }
    m_loginName = loginName;
    m_wasLocked = false;
    m_lockedAtLastTry = false;
    emit loginNameChanged();
    // Nothing read so far is about this account, so a first read that finds
    // faillock writing must not keep the last account's lock.
    if (m_state != State::Unknown) {
        show(State::Unknown, 0);
        emit stateChanged();
    }
    refresh();
}

QUrl AccountLock::settingsFile() const {
    return m_settingsFile;
}

void AccountLock::setSettingsFile(const QUrl& file) {
    if (file == m_settingsFile) {
        return;
    }
    m_settingsFile = file;
    emit settingsFileChanged();
    refresh();
}

QDateTime AccountLock::fixedTime() const {
    return m_fixedTime;
}

void AccountLock::setFixedTime(const QDateTime& time) {
    if (time == m_fixedTime) {
        return;
    }
    m_fixedTime = time;
    emit fixedTimeChanged();
    refresh();
}

AccountLock::State AccountLock::state() const {
    return m_state;
}

bool AccountLock::locked() const {
    return m_state == State::Locked || m_state == State::LockedUnderAMinute ||
           m_state == State::LockedUntilReset || m_state == State::LockedForUnknownTime;
}

int AccountLock::minutesLeft() const {
    return m_minutesLeft;
}

int AccountLock::lockMinutes() const {
    return m_lockMinutes;
}

bool AccountLock::lockedAtLastTry() const {
    return m_lockedAtLastTry;
}

void AccountLock::noteTry() {
    refresh();
    m_lockedAtLastTry = locked();
}

void AccountLock::refresh() {
    const State state = m_state;
    const int minutesLeft = m_minutesLeft;
    const int lockMinutes = m_lockMinutes;
    workOut();
    // Read again each second only while there is something to count down,
    // or faillock was writing.
    if (locked() || m_busy) {
        m_timer.start();
    } else {
        m_timer.stop();
    }
    // Only a real change is announced, so the words move once a minute.
    if (state != m_state || minutesLeft != m_minutesLeft || lockMinutes != m_lockMinutes) {
        emit stateChanged();
    }
}

void AccountLock::workOut() {
    m_busy = false;
    if (m_loginName.isEmpty()) {
        show(State::Unknown, 0);
        return;
    }
    const std::optional<FaillockSettings> settings =
        FaillockSettings::read(m_settingsFile.toLocalFile());
    if (!settings) {
        show(State::Unknown, 0);
        return;
    }
    const TallyFile::Result tally = TallyFile::read(settings->dir, m_loginName);
    if (tally.outcome == TallyFile::Outcome::Busy) {
        // Keep what was last read; the timer reads again in a second.
        m_busy = true;
        return;
    }
    if (tally.outcome == TallyFile::Outcome::Unknown) {
        show(State::Unknown, 0);
        return;
    }

    m_lockMinutes = wholeMinutes(settings->unlockTime);
    const qint64 now =
        m_fixedTime.isValid() ? m_fixedTime.toSecsSinceEpoch() : QDateTime::currentSecsSinceEpoch();
    const LockRule::Result rule = LockRule::decide(tally.failureTimes, *settings, now);
    switch (rule.kind) {
    case LockRule::Kind::Open:
        show(m_wasLocked ? State::OpenAgain : State::Open, 0);
        return;
    case LockRule::Kind::OneTryLeft:
        // With deny = 1 a lock that runs out leaves one try; the screen
        // first says the lock is over.
        if (m_wasLocked) {
            show(State::OpenAgain, 0);
            return;
        }
        // "for 1 minutes" would read wrong with no translation loaded, so a
        // lock of a minute or less, or until a reset, is not said in minutes.
        show(m_lockMinutes >= 2 ? State::OneTryLeft : State::OneTryLeftNoTime, 0);
        return;
    case LockRule::Kind::Locked:
        m_wasLocked = true;
        // At 0 seconds faillock still refuses, so that is under a minute too.
        if (rule.secondsLeft <= secondsInMinute) {
            show(State::LockedUnderAMinute, 1);
        } else {
            show(State::Locked, wholeMinutes(rule.secondsLeft));
        }
        return;
    case LockRule::Kind::LockedUntilReset:
        m_wasLocked = true;
        show(State::LockedUntilReset, 0);
        return;
    case LockRule::Kind::LockedForUnknownTime:
        m_wasLocked = true;
        show(State::LockedForUnknownTime, 0);
        return;
    }
}

void AccountLock::show(State state, int minutesLeft) {
    m_state = state;
    m_minutesLeft = minutesLeft;
    if (state == State::Unknown) {
        m_lockMinutes = 0;
    }
}
