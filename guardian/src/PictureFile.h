// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

// A grown-up's chosen picture made ready for AccountsService (ADR-0043): a
// square from the middle of the image, turned the way the camera meant,
// at most `side` pixels wide, written as PNG. AccountsService refuses a
// file over a megabyte, and the login screen shows the picture small, so
// a photo straight from a phone is made small first.
namespace PictureFile {

inline constexpr int side = 256;

// False when `source` is not an image Qt can read, or `target` cannot be
// written.
bool prepare(const QString& source, const QString& target);

} // namespace PictureFile
