// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include <optional>

// The pam_faillock settings the login screen needs to work out a lock the
// way faillock does (ADR-0052), and Family needs to say how long a lock it
// reports lasted (ADR-0054). It reads /etc/security/faillock.conf the way
// linux-pam 1.7.2 does (faillock_config.c), and starts from the same
// defaults.
struct FaillockSettings {
    QString dir = QStringLiteral("/var/run/faillock");
    int deny = 3;
    qint64 failInterval = 900;
    // Seconds; 0 is faillock's "never", locked until someone resets the count.
    qint64 unlockTime = 600;

    // The defaults when there is no file. Nothing when the file cannot be
    // read, or holds a line faillock would refuse or that this copy of the
    // rule does not follow (admin_group, even_deny_root, root_unlock_time,
    // anything unknown). Then the screen never claims a lock it cannot be
    // sure of.
    static std::optional<FaillockSettings> read(const QString& path);
};
