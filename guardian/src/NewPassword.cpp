// SPDX-License-Identifier: Apache-2.0
#include "NewPassword.h"

#include <QList>

#include <algorithm>

// Each character is judged whole, so one outside the first 65,536, such as
// an emoji, counts as the one character it is.
std::optional<NewPassword::Problem> NewPassword::check(const QString& password) {
    if (password.isEmpty()) {
        return Problem::Empty;
    }
    // Half of a pair with no other half is no character at all.
    if (!password.isValidUtf16()) {
        return Problem::Unprintable;
    }
    const QList<uint> characters = password.toUcs4();
    if (characters.size() > maxLength) {
        return Problem::TooLong;
    }
    if (!std::ranges::all_of(characters, [](uint character) {
            return QChar::isPrint(static_cast<char32_t>(character));
        })) {
        return Problem::Unprintable;
    }
    return std::nullopt;
}
