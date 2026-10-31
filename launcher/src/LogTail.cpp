// SPDX-License-Identifier: Apache-2.0
#include "LogTail.h"

#include <QFile>

#include <algorithm>
#include <utility>

namespace {
// Steam's logs grow by a few lines a second, its content log a few more
// while it downloads. A log that has gained more than this since the last
// look is read from its last part.
constexpr qint64 mostReadAtOnce = qint64{4} * 1024 * 1024;
} // namespace

LogTail::LogTail(QString path) : m_path(std::move(path)) {}

void LogTail::skipToEnd() {
    const QFile log(m_path);
    m_offset = log.exists() ? log.size() : 0;
    m_startedAgain = false;
}

QList<QByteArray> LogTail::newLines() {
    m_startedAgain = false;
    QFile log(m_path);
    if (!log.open(QIODevice::ReadOnly)) {
        return {};
    }
    const qint64 size = log.size();
    if (size < m_offset) {
        m_offset = 0;
        m_startedAgain = true;
    }
    const qint64 from = std::max(m_offset, size - mostReadAtOnce);
    if (!log.seek(from)) {
        return {};
    }
    const QByteArray gained = log.read(size - from);
    // A line still being written is read whole next time.
    const qsizetype end = gained.lastIndexOf('\n') + 1;
    m_offset = from + end;
    if (end == 0) {
        return {};
    }
    return gained.first(end - 1).split('\n');
}

bool LogTail::startedAgain() const {
    return m_startedAgain;
}
