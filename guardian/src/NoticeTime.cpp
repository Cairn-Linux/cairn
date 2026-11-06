// SPDX-License-Identifier: Apache-2.0
#include "NoticeTime.h"

#include <utility>

NoticeTime::NoticeTime(QTimeZone zone, QLocale locale)
    : m_zone(std::move(zone)), m_locale(std::move(locale)) {}

QDateTime NoticeTime::local(qint64 time) const {
    return QDateTime::fromMSecsSinceEpoch(time / 1000, m_zone);
}

QString NoticeTime::number(qint64 count) const {
    return m_locale.toString(count);
}

QString NoticeTime::times(int count) const {
    if (count == 1) {
        return tr("once");
    }
    if (count == 2) {
        return tr("twice");
    }
    return tr("%1 times").arg(number(count));
}

QString NoticeTime::moreTimes(int count) const {
    if (count == 1) {
        return tr("once more");
    }
    if (count == 2) {
        return tr("twice more");
    }
    return tr("%1 more times").arg(number(count));
}

QString NoticeTime::minutes(qint64 seconds) const {
    const qint64 count = (seconds + 59) / 60;
    if (count <= 1) {
        return tr("1 minute");
    }
    return tr("%1 minutes").arg(number(count));
}

// Qt writes "5:41 AM" with a narrow no-break space (U+202F) that the
// brand font lacks; the fallback font for that one character made every
// line of a notice half again as tall (cairn-min, 2026-11-06). An ordinary
// no-break space keeps the time on one line in the brand font.
QString NoticeTime::clock(qint64 time) const {
    return m_locale.toString(local(time).time(), QLocale::ShortFormat)
        .replace(QChar(0x202f), QChar(0x00a0));
}

QString NoticeTime::day(qint64 time) const {
    return m_locale.toString(local(time).date(), QLocale::LongFormat);
}

QString NoticeTime::when(qint64 first, qint64 last) const {
    if (local(first).date() != local(last).date()) {
        return tr("from %1 at %2 to %3 at %4")
            .arg(day(first), clock(first), day(last), clock(last));
    }
    if (clock(first) == clock(last)) {
        return tr("on %1, at %2").arg(day(first), clock(first));
    }
    return tr("on %1, from %2 to %3").arg(day(first), clock(first), clock(last));
}
