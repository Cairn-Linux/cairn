// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>

// A file root writes and every account reads, mode 0644, such as a games
// file the launcher reads at a child's login.
namespace ReadableFile {

enum class Written : std::uint8_t { Changed, Unchanged, Failed };

// Writes the bytes beside the file and renames them into place, so a reader
// sees the old file or the new one and never half of one. A file that
// already holds the same bytes is left alone only when it belongs to the
// writer and is mode 0644. An unsafe mode on the writer's own file is
// repaired to 0644 and then counts as Changed; a file owned by someone
// else is replaced, so its new owner is the writer.
Written write(const QString& path, const QByteArray& bytes);

} // namespace ReadableFile
