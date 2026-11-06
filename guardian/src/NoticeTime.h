// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QString>
#include <QTimeZone>

// How many times and when, in the Notices page's words (ADR-0054): "once",
// "3 times", "10 minutes", "on Wednesday, 4 November 2026, from 16:02 to
// 16:05". Dates, times and numbers are the locale's own, never built by
// hand. Times are microseconds since 1970, as the journal keeps them.
class NoticeTime {
    Q_DECLARE_TR_FUNCTIONS(NoticeTime)

public:
    NoticeTime(QTimeZone zone, QLocale locale);

    QString times(int count) const;
    QString moreTimes(int count) const;
    // Whole minutes, rounded up, as faillock's settings are seconds.
    QString minutes(qint64 seconds) const;
    QString number(qint64 count) const;
    // From the first time to the last, said once when both are the same
    // minute.
    QString when(qint64 first, qint64 last) const;
    QString clock(qint64 time) const;
    QString day(qint64 time) const;

private:
    QDateTime local(qint64 time) const;

    QTimeZone m_zone;
    QLocale m_locale;
};
