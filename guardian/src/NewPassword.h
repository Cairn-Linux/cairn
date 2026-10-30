// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include <cstdint>
#include <optional>

// The one rule for a grown-up's new password (ADR-0043), which the Family
// window checks before it asks for the current password and cairn-family
// checks again before it changes anything. It sets no length or strength
// of its own beyond a limit no one types: the family decides that.
namespace NewPassword {

enum class Problem : std::uint8_t {
    Empty,
    // More than maxLength characters.
    TooLong,
    // A tab, a line break or another character that is not drawn. chpasswd
    // reads one account to a line, and a password box cannot type them.
    Unprintable,
};

inline constexpr qsizetype maxLength = 128;

// Nothing when the password may be used.
std::optional<Problem> check(const QString& password);

} // namespace NewPassword
