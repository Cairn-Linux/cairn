// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// The lines a log file gains, read a part at a time, as the launcher reads
// Steam's console and content logs (ADR-0036, #206).
//
// Only what the log gains after skipToEnd() counts. A line still being
// written is read once it is whole. A log smaller than what was read before
// is a new one, begun when its program started again, and all of it is new.
// SteamLaunchLog and SteamContentLog each keep one and read its lines in
// their own way.
class LogTail {
public:
    explicit LogTail(QString path);

    // Only what the log gains from now counts.
    void skipToEnd();
    // The lines the log has gained since the last call, oldest first, but
    // not one still being written; none when it cannot be read. After a
    // gain of more than 4 MB only the last part is read, and its first line
    // may be cut part-way.
    QList<QByteArray> newLines();
    // The last newLines() found a new log and read it from its start.
    bool startedAgain() const;

private:
    QString m_path;
    qint64 m_offset = 0;
    bool m_startedAgain = false;
};
