// SPDX-License-Identifier: Apache-2.0
#include "NoticeWords.h"

#include "FamilyRole.h"

#include <QStringList>

#include <utility>

namespace {

constexpr qint64 microseconds = 1000000;
constexpr qint64 secondsInADay = 86400;

bool isFamily(const Account& account) {
    const FamilyRole role = FamilyRole::fromGroups(account.groups);
    return role.kind == FamilyRole::Kind::Guardian || role.kind == FamilyRole::Kind::Child;
}

// A child at level 1 or 2 has no password to type (ADR-0039), so every try
// on their account fails, the "right" one too.
bool hasNoPassword(const std::optional<Account>& account) {
    if (!account) {
        return false;
    }
    const FamilyRole role = FamilyRole::fromGroups(account->groups);
    return role.kind == FamilyRole::Kind::Child && role.level &&
           *role.level <= FamilyRole::highestKioskLevel;
}

} // namespace

// An account's full name is text its owner can change with no password
// (ADR-0043), so a child could call themselves "Mum". Its login name, which
// only an administrator can change, always goes beside it. Control and
// format characters, such as line breaks and bidi overrides, are dropped so
// the full name cannot hide or turn round the login name after it.
QString NoticeWords::nameOf(const Account& account) {
    QString fullName;
    for (const QChar character : account.fullName) {
        const QChar::Category kind = character.category();
        if (kind != QChar::Other_Control && kind != QChar::Other_Format) {
            fullName.append(character);
        }
    }
    fullName = fullName.trimmed();
    if (fullName.isEmpty() || fullName == account.login) {
        return account.login;
    }
    return tr("%1 (%2)", "an account's full name, then its login name")
        .arg(fullName, account.login);
}

NoticeWords::NoticeWords(unsigned int ownUid, std::optional<FaillockSettings> settings,
                         QTimeZone zone, QLocale locale)
    : m_ownUid(ownUid), m_settings(std::move(settings)),
      m_time(std::move(zone), std::move(locale)) {}

QString NoticeWords::passwordOf(const std::optional<Account>& account) const {
    if (!account) {
        return tr("The password of a login name that is not an account on this computer");
    }
    if (account->uid == m_ownUid) {
        return tr("Your password");
    }
    if (isFamily(*account)) {
        return tr("The password of %1").arg(nameOf(*account));
    }
    return tr("The password of the account %1").arg(account->login);
}

QString NoticeWords::accountOf(const Account& account) const {
    if (account.uid == m_ownUid) {
        return tr("Your account");
    }
    if (isFamily(account)) {
        return tr("The account of %1").arg(nameOf(account));
    }
    return tr("The account %1").arg(account.login);
}

QString NoticeWords::place(const TryEpisode& episode) const {
    QString words;
    switch (episode.place) {
    case TryPlace::LoginScreen:
        words = tr("at the login screen");
        break;
    case TryPlace::OwnLogin:
        words = episode.account && episode.account->uid == m_ownUid
                    ? tr("in your own login, at the lock screen or in another program")
                    : tr("in their own login, at the lock screen or in another program");
        break;
    case TryPlace::Sudo:
        words = tr("with sudo");
        break;
    case TryPlace::Su:
        words = tr("with su");
        break;
    case TryPlace::PasswordBox:
        words = tr("in a password box");
        break;
    case TryPlace::Console:
        words = tr("at a text console");
        break;
    case TryPlace::Other:
        words = episode.program.isEmpty() ? tr("by another program")
                                          : tr("by another program (%1)").arg(episode.program);
        break;
    }
    if (!episode.from) {
        return words;
    }
    if (episode.from->login.isEmpty()) {
        return tr("%1, from another login").arg(words);
    }
    if (episode.from->uid == m_ownUid) {
        return tr("%1, from your login").arg(words);
    }
    return tr("%1, from the login of %2").arg(words, nameOf(*episode.from));
}

// How long the lock lasted. "That" is the wrong passwords before it, or,
// on a notice of a lock alone, the lock.
QString NoticeWords::lock(const TryEpisode& episode) const {
    const bool alone = episode.wrong == 0 && episode.whileLocked == 0;
    if (!m_settings) {
        return alone ? QString()
                     : tr("That locked the account for a while, at every password prompt.");
    }
    if (m_settings->unlockTime == 0) {
        return tr("That locked the account at every password prompt, until someone with an "
                  "administrator's account reset its count of wrong tries.");
    }
    const QString until = m_time.clock(*episode.lockTime + (m_settings->unlockTime * microseconds));
    return tr("That locked the account for %1, at every password prompt. The right password did "
              "not work until about %2.")
        .arg(m_time.minutes(m_settings->unlockTime), until);
}

// In the order it happened: the wrong passwords, the lock they caused,
// then the tries refused while it held. A lock goes only with wrong
// passwords, or alone: tries refused while an account was locked cannot
// have locked it, so they never come with "That locked the account".
QString NoticeWords::sentence(const TryEpisode& episode) const {
    QStringList sentences;
    const QString whenWords = m_time.when(episode.first, episode.last);
    const bool locked = episode.lockTime && (episode.wrong > 0 || episode.whileLocked == 0);
    if (episode.wrong > 0) {
        if (hasNoPassword(episode.account)) {
            // Only a child has no password, so never "your account".
            sentences << tr("Someone tried to use the account of %1 %2 %3, %4.")
                             .arg(nameOf(*episode.account), m_time.times(episode.wrong),
                                  place(episode), whenWords);
        } else {
            sentences << tr("%1 was typed wrong %2 %3, %4.")
                             .arg(passwordOf(episode.account), m_time.times(episode.wrong),
                                  place(episode), whenWords);
        }
    } else if (episode.whileLocked > 0 && episode.account) {
        sentences << tr("%1 was tried %2 %3 while it was locked, %4.")
                         .arg(accountOf(*episode.account), m_time.times(episode.whileLocked),
                              place(episode), whenWords);
    } else if (episode.account) {
        sentences << tr("%1 was locked %2, %3.")
                         .arg(accountOf(*episode.account), place(episode), whenWords);
    }
    if (locked) {
        const QString lockWords = lock(episode);
        if (!lockWords.isEmpty()) {
            sentences << lockWords;
        }
        if (m_settings && episode.wrong < m_settings->deny) {
            sentences << tr("Wrong tries at other prompts, and wrong fingers at the lock screen, "
                            "count toward a lock too.");
        }
    }
    if (episode.wrong > 0 && episode.whileLocked > 0) {
        // Without this notice's lock, a lock from elsewhere refused them,
        // before, between or after the wrong ones.
        sentences << (locked ? tr("It was tried %1 while it was locked.")
                                   .arg(m_time.moreTimes(episode.whileLocked))
                             : tr("In that time it was also tried %1 while the account was "
                                  "locked.")
                                   .arg(m_time.times(episode.whileLocked)));
    }
    return sentences.join(QLatin1Char(' '));
}

QString NoticeWords::footer() const {
    if (!m_settings || m_settings->unlockTime == 0) {
        return tr("A few wrong tries in a row, by password or by finger, lock an account. If this "
                  "was you, there is nothing to do. If it was not, someone may be guessing: a "
                  "password nobody has watched you type is the best protection. Children do not "
                  "see this page.");
    }
    return tr("%1 wrong tries within %2, by password or by finger, lock an account for %3. If "
              "this was you, there is nothing to do. If it was not, someone may be guessing: a "
              "password nobody has watched you type is the best protection. Children do not see "
              "this page.")
        .arg(m_time.number(m_settings->deny), m_time.minutes(m_settings->failInterval),
             m_time.minutes(m_settings->unlockTime));
}

QString NoticeWords::recordStart(qint64 oldest, qint64 now, int days) const {
    if (oldest <= now - (days * secondsInADay * microseconds)) {
        return {};
    }
    return tr("This computer's record goes back only to %1.").arg(m_time.day(oldest));
}

QString NoticeWords::notShown(int count) const {
    if (count <= 0) {
        return {};
    }
    if (count == 1) {
        return tr("One older notice is not shown.");
    }
    return tr("%1 older notices are not shown.").arg(m_time.number(count));
}
