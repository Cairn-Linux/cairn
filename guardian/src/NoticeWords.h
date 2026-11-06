// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "FaillockSettings.h"
#include "NoticeTime.h"
#include "TryNotices.h"

#include <QCoreApplication>
#include <QLocale>
#include <QString>
#include <QTimeZone>

#include <optional>

// The sentences of the Notices page (ADR-0054), whole and translatable. A
// name in them is always an account's own name on this computer, never
// text from the journal, and a full name always has its login name beside
// it. Dates and times are the locale's own.
class NoticeWords {
    Q_DECLARE_TR_FUNCTIONS(NoticeWords)

public:
    // The Guardian reading the page, so their own account is "your".
    NoticeWords(unsigned int ownUid, std::optional<FaillockSettings> settings, QTimeZone zone,
                QLocale locale = QLocale());

    // One notice, in one to three sentences.
    QString sentence(const TryEpisode& episode) const;
    // Once, under the list: how faillock locks, and what to do.
    QString footer() const;
    // When the journal's oldest entry is newer than `days` before `now`,
    // the sentence that says so; empty otherwise. Both in microseconds.
    QString recordStart(qint64 oldest, qint64 now, int days) const;
    QString notShown(int count) const;

private:
    static QString nameOf(const Account& account);
    QString passwordOf(const std::optional<Account>& account) const;
    QString accountOf(const Account& account) const;
    QString place(const TryEpisode& episode) const;
    QString lock(const TryEpisode& episode) const;

    unsigned int m_ownUid;
    std::optional<FaillockSettings> m_settings;
    NoticeTime m_time;
};
