// SPDX-License-Identifier: Apache-2.0
#include "OsRelease.h"

#include <QChar>
#include <QFile>
#include <QFileInfo>
#include <QStringView>

#include <algorithm>

namespace {

// The four characters a backslash may stand before in double quotes.
bool canFollowBackslash(QChar character) {
    return character == u'"' || character == u'\\' || character == u'$' || character == u'`';
}

// The text between double quotes, or nothing when they never close or
// something is written after them.
std::optional<QString> doubleQuoted(QStringView written) {
    QString value;
    for (qsizetype at = 1; at < written.size(); ++at) {
        const QChar character = written.at(at);
        if (character == u'"') {
            return at == written.size() - 1 ? std::optional<QString>(value) : std::nullopt;
        }
        if (character == u'\\' && at + 1 < written.size() &&
            canFollowBackslash(written.at(at + 1))) {
            ++at;
        }
        value.append(written.at(at));
    }
    return std::nullopt;
}

// The text between single quotes, where every character is itself.
std::optional<QString> singleQuoted(QStringView written) {
    const qsizetype closing = written.indexOf(u'\'', 1);
    if (closing < 1 || closing != written.size() - 1) {
        return std::nullopt;
    }
    return written.mid(1, closing - 1).toString();
}

// What is written after the equals sign, as a value that can go on screen.
std::optional<QString> valueFrom(QStringView written) {
    std::optional<QString> value;
    if (written.startsWith(u'"')) {
        value = doubleQuoted(written);
    } else if (written.startsWith(u'\'')) {
        value = singleQuoted(written);
    } else {
        value = written.toString();
    }
    const auto isControl = [](QChar character) {
        return character.category() == QChar::Other_Control;
    };
    if (!value || value->size() > OsRelease::kLongestValue ||
        std::ranges::any_of(*value, isControl)) {
        return std::nullopt;
    }
    return value;
}

} // namespace

std::optional<OsRelease> OsRelease::read(const QString& path) {
    QFile file(path);
    if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly) || file.size() > kFileLimit) {
        return std::nullopt;
    }
    const QString text = QString::fromUtf8(file.read(kFileLimit));
    OsRelease release;
    for (const QStringView whole : QStringView(text).split(u'\n')) {
        const QStringView line = whole.trimmed();
        const qsizetype equals = line.indexOf(u'=');
        if (line.startsWith(u'#') || equals < 1) {
            continue;
        }
        const QString key = line.left(equals).toString();
        // A later line for a key wins, also when its value cannot be used.
        const std::optional<QString> value = valueFrom(line.mid(equals + 1));
        if (value) {
            release.m_values.insert(key, *value);
        } else {
            release.m_values.remove(key);
        }
    }
    return release;
}

std::optional<QString> OsRelease::value(const QString& key) const {
    const auto found = m_values.constFind(key);
    if (found == m_values.constEnd()) {
        return std::nullopt;
    }
    return *found;
}
