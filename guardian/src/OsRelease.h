// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QHash>
#include <QString>

#include <optional>

// The KEY=value lines of an os-release file (os-release(5)), which says
// which system a computer runs. Comments, blank lines and lines that are
// not an assignment are passed over. A value is bare, in single quotes or
// in double quotes; inside double quotes a backslash before a quote, a
// backslash, a dollar or a backtick gives that character. Nothing is
// expanded: the file is read as text, never run as a shell would run it.
// It knows nothing about Cairn.
class OsRelease {
public:
    // The image's own file is under one kilobyte.
    static constexpr qint64 kFileLimit = 1 << 16;
    // Values go on screen, so one longer than this counts as absent.
    static constexpr qsizetype kLongestValue = 200;

    // The file's assignments, or nothing when the path is not a plain file
    // of at most kFileLimit bytes or cannot be read. A link is followed,
    // because /etc/os-release is one.
    static std::optional<OsRelease> read(const QString& path);

    // What the file gives this key. A key given twice keeps its last line.
    // Nothing when the key is not there, or when its value has a control
    // character, a quote that never closes, or is longer than kLongestValue.
    // Other characters that cannot be seen, such as a right-to-left mark,
    // are kept: a caller that shows a value checks its characters itself,
    // as AboutPage does.
    std::optional<QString> value(const QString& key) const;

private:
    QHash<QString, QString> m_values;
};
