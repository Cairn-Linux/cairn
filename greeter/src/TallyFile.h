// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QList>
#include <QString>

#include <cstdint>

// pam_faillock's record of one account's wrong tries, read without
// changing it (ADR-0052). The file is <dir>/<login>: 64-byte records in
// the computer's own byte order, with no header (linux-pam 1.7.2,
// faillock.h). Only records faillock still counts are kept.
class TallyFile {
public:
    enum class Outcome : std::uint8_t {
        Read,    // failureTimes holds every counted try, perhaps none
        Busy,    // faillock is writing it just now; read again soon
        Unknown, // it cannot be read, so nothing is known
    };

    struct Result {
        Outcome outcome = Outcome::Unknown;
        // When each counted wrong try happened, in seconds since 1970.
        QList<qint64> failureTimes;
    };

    static Result read(const QString& dir, const QString& loginName);
};
