// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "FaillockSettings.h"

#include <QtTypes>

#include <cstdint>
#include <vector>

// One account's count of wrong passwords as pam_faillock 1.7.2 keeps it
// (pam_faillock.c, check_tally and write_tally), worked out again from the
// tries in the journal (ADR-0054). It follows faillock's two rules as they
// are, the faults too: check_tally counts only the records still valid
// within fail_interval of the newest one, and locks when there are deny of
// them; write_tally announces a lock, with its audit record and its line,
// only when its own count equals deny, and that count takes in records an
// ended lock left behind. So a lock that starts within fail_interval of an
// ended one is often never announced, and once in a while a lock is
// announced that never was. Times are microseconds; faillock keeps
// seconds, and so does this.
class FaillockTally {
public:
    enum class Outcome : std::uint8_t {
        Wrong,          // counted; the account is not locked
        Locked,         // counted, and that locked it; faillock said so
        SilentlyLocked, // counted, and that locked it; faillock said nothing
        // counted, and faillock said it locked, but its own lock rule
        // says it did not
        AnnouncedOnly,
        Refused, // the account was locked: not counted, whatever was typed
    };

    explicit FaillockTally(FaillockSettings rule = FaillockSettings());

    // A failed password check, as faillock's preauth and authfail see it.
    Outcome fail(qint64 time);
    // Whether faillock refuses every password at that time.
    bool refuses(qint64 time) const;
    // When the lock that holds began: the newest record's time.
    qint64 lockStart() const;
    // A good password where faillock's account part ran, or a reset: the
    // count is emptied.
    void reset();
    // The journal says faillock locked here, after tries this count did
    // not see, such as wrong fingers: records enough to lock are added.
    void lockAt(qint64 time);

private:
    struct Record {
        qint64 second = 0;
        bool valid = true;
    };

    // check_tally's count, and the newest valid record's time.
    int counted(qint64& newest) const;
    bool expired(qint64 newest, qint64 now) const;

    FaillockSettings m_rule;
    // faillock's file: never more than deny and one records, since a new
    // record takes the place of the oldest when that one is not valid.
    std::vector<Record> m_records;
};
