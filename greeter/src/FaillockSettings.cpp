// SPDX-License-Identifier: Apache-2.0
#include "FaillockSettings.h"

#include <QByteArray>
#include <QFile>

namespace {

// faillock_config.h: no time setting may be longer than seven days.
constexpr qint64 maxTimeInterval = 604800;
// deny is an unsigned short in faillock.
constexpr qint64 maxDeny = 65535;

// isspace() in the C locale, which is what faillock's parser uses.
bool isSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

// A whole number of plain digits no bigger than max. faillock's sscanf
// would take "10abc" as 10; this refuses it, so an odd line means "not
// sure" rather than a guess.
std::optional<qint64> number(const QByteArray& value, qint64 max) {
    if (value.isEmpty() || value.size() > 7) {
        return std::nullopt;
    }
    qint64 result = 0;
    for (const char c : value) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        result = (result * 10) + (c - '0');
    }
    if (result > max) {
        return std::nullopt;
    }
    return result;
}

// Takes one setting into settings. False for a setting faillock would
// refuse, or one that changes the rule in a way this copy does not follow.
bool take(FaillockSettings& settings, const QByteArray& key, const QByteArray& value) {
    if (key == "dir") {
        if (!value.startsWith('/')) {
            return false;
        }
        settings.dir = QString::fromUtf8(value);
        return true;
    }
    if (key == "deny" || key == "fail_interval" || key == "unlock_time") {
        const bool never = key == "unlock_time" && value == "never";
        const std::optional<qint64> found =
            never ? std::optional<qint64>(0)
                  : number(value, key == "deny" ? maxDeny : maxTimeInterval);
        if (!found) {
            return false;
        }
        if (key == "deny") {
            settings.deny = static_cast<int>(*found);
        } else if (key == "fail_interval") {
            settings.failInterval = *found;
        } else {
            settings.unlockTime = *found;
        }
        return true;
    }
    // These change what faillock logs or prints, never when it locks.
    return key == "audit" || key == "silent" || key == "no_log_info" || key == "local_users_only" ||
           key == "nodelay";
}

} // namespace

std::optional<FaillockSettings> FaillockSettings::read(const QString& path) {
    FaillockSettings settings;
    QFile file(path);
    // faillock uses its defaults when the file is not there.
    if (!file.exists()) {
        return settings;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        const qsizetype hash = line.indexOf('#');
        if (hash >= 0) {
            line.truncate(hash);
        }
        line = line.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        // As read_config_file splits a line: the key ends at a space or an
        // '=', then spaces and at most one '=' are skipped.
        qsizetype at = 0;
        while (at < line.size() && !isSpace(line[at]) && line[at] != '=') {
            ++at;
        }
        const QByteArray key = line.left(at);
        bool equals = false;
        if (at < line.size()) {
            equals = line[at] == '=';
            ++at;
        }
        while (at < line.size()) {
            if (line[at] == '=' && !equals) {
                equals = true;
            } else if (!isSpace(line[at])) {
                break;
            }
            ++at;
        }
        if (!take(settings, key, line.mid(at))) {
            return std::nullopt;
        }
    }
    if (file.error() != QFileDevice::NoError) {
        return std::nullopt;
    }
    return settings;
}
