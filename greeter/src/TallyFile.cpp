// SPDX-License-Identifier: Apache-2.0
#include "TallyFile.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <cstring>
#include <limits>
#include <sys/file.h>

namespace {

// struct tally in faillock.h: char source[52], uint16_t reserved,
// uint16_t status, uint64_t time.
constexpr qsizetype recordSize = 64;
constexpr qsizetype statusOffset = 54;
constexpr qsizetype timeOffset = 56;
// faillock reads no more records than this (read_tally).
constexpr qsizetype maxRecords = 1024;
// TALLY_STATUS_VALID. The other bits say where the try came from.
constexpr std::uint16_t statusValid = 0x1;

TallyFile::Result unknown() {
    return {.outcome = TallyFile::Outcome::Unknown, .failureTimes = {}};
}

} // namespace

TallyFile::Result TallyFile::read(const QString& dir, const QString& loginName) {
    if (loginName.isEmpty() || loginName.contains(QLatin1Char('/')) ||
        loginName == QLatin1String(".") || loginName == QLatin1String("..")) {
        return unknown();
    }
    const QString path =
        dir.endsWith(QLatin1Char('/')) ? dir + loginName : dir + QLatin1Char('/') + loginName;
    const QFileInfo info(path);

    // No file means no wrong tries, as faillock reads it. But only in a
    // folder that is there and can be read: in the wrong folder every
    // account would look unlocked.
    if (!info.exists()) {
        const QFileInfo folder(dir);
        if (folder.isDir() && folder.isReadable() && folder.isExecutable()) {
            return {.outcome = Outcome::Read, .failureTimes = {}};
        }
        return unknown();
    }

    QFile file(path);
    if (!info.isFile() || !file.open(QIODevice::ReadOnly)) {
        return unknown();
    }
    // faillock rewrites the file from the start under an exclusive lock, so
    // a read while it holds one could see half of each. Never wait for it:
    // the screen must not freeze. A lock that cannot be taken for any
    // other reason means the read could be half-written, so "not sure".
    if (flock(file.handle(), LOCK_SH | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK || errno == EINTR) {
            return {.outcome = Outcome::Busy, .failureTimes = {}};
        }
        return unknown();
    }
    const QByteArray bytes = file.read(recordSize * maxRecords);
    if (file.error() != QFileDevice::NoError) {
        return unknown();
    }

    // A piece of a record at the end is left out, as faillock leaves it.
    Result result{.outcome = Outcome::Read, .failureTimes = {}};
    for (qsizetype at = 0; at + recordSize <= bytes.size(); at += recordSize) {
        std::uint16_t status = 0;
        std::uint64_t time = 0;
        std::memcpy(&status, bytes.constData() + at + statusOffset, sizeof status);
        std::memcpy(&time, bytes.constData() + at + timeOffset, sizeof time);
        // A bit test, as faillock's: a try from a terminal is 0x5 and one
        // from another computer 0x3.
        if ((status & statusValid) == 0) {
            continue;
        }
        // No clock gives a time this large, so the file is not what it seems.
        if (time > static_cast<std::uint64_t>(std::numeric_limits<qint64>::max())) {
            return unknown();
        }
        result.failureTimes.append(static_cast<qint64>(time));
    }
    return result;
}
