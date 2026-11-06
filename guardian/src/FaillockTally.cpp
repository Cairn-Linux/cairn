// SPDX-License-Identifier: Apache-2.0
#include "FaillockTally.h"

#include <algorithm>
#include <utility>

namespace {

constexpr qint64 microseconds = 1000000;

} // namespace

FaillockTally::FaillockTally(FaillockSettings rule) : m_rule(std::move(rule)) {}

int FaillockTally::counted(qint64& newest) const {
    newest = 0;
    for (const Record& record : m_records) {
        if (record.valid && record.second > newest) {
            newest = record.second;
        }
    }
    int failures = 0;
    for (const Record& record : m_records) {
        if (record.valid && newest - record.second < m_rule.failInterval) {
            ++failures;
        }
    }
    return failures;
}

// A lock ends unlock_time after its newest record; "never" never ends.
bool FaillockTally::expired(qint64 newest, qint64 now) const {
    return m_rule.unlockTime != 0 && newest + m_rule.unlockTime < now;
}

bool FaillockTally::refuses(qint64 time) const {
    qint64 newest = 0;
    const int failures = counted(newest);
    return m_rule.deny > 0 && failures >= m_rule.deny && !expired(newest, time / microseconds);
}

qint64 FaillockTally::lockStart() const {
    qint64 newest = 0;
    counted(newest);
    return newest * microseconds;
}

void FaillockTally::reset() {
    m_records.clear();
}

FaillockTally::Outcome FaillockTally::fail(qint64 time) {
    const qint64 now = time / microseconds;
    qint64 newest = 0;
    bool unlocked = false;
    if (m_rule.deny > 0 && counted(newest) >= m_rule.deny) {
        if (!expired(newest, now)) {
            return Outcome::Refused;
        }
        // The lock ended: check_tally marks it so, and write_tally then
        // takes every record out of the count.
        unlocked = true;
    }
    // write_tally's own count. It never looks at whether a record is
    // still valid, which is faillock's fault: records an ended lock left
    // behind count until they are fail_interval old.
    int announced = 0;
    std::size_t oldest = 0;
    for (std::size_t i = 0; i < m_records.size(); ++i) {
        Record& record = m_records[i];
        if (record.second < m_records[oldest].second) {
            oldest = i;
        }
        if (unlocked || now - record.second >= m_rule.failInterval) {
            record.valid = false;
        } else {
            ++announced;
        }
    }
    if (m_records.empty() || m_records[oldest].valid) {
        m_records.push_back(Record{.second = now, .valid = true});
    } else {
        m_records[oldest] = Record{.second = now, .valid = true};
    }
    ++announced;
    const bool says = m_rule.deny > 0 && announced == m_rule.deny;
    if (!refuses(time)) {
        return says ? Outcome::AnnouncedOnly : Outcome::Wrong;
    }
    return says ? Outcome::Locked : Outcome::SilentlyLocked;
}

void FaillockTally::lockAt(qint64 time) {
    if (m_rule.deny <= 0) {
        return;
    }
    qint64 newest = 0;
    counted(newest);
    // Never older than a record already there, so each one added counts.
    const qint64 stamp = std::max(time / microseconds, newest);
    while (!refuses(stamp * microseconds)) {
        m_records.push_back(Record{.second = stamp, .valid = true});
    }
}
