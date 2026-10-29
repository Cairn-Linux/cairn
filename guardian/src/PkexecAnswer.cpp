// SPDX-License-Identifier: Apache-2.0
#include "PkexecAnswer.h"

namespace {

constexpr int kDismissed = 126;
constexpr int kNotAuthorized = 127;

} // namespace

std::optional<QString> PkexecAnswer::fromExitCode(int exitCode) {
    if (exitCode == kDismissed) {
        return QStringLiteral("dismissed");
    }
    if (exitCode == kNotAuthorized) {
        return QStringLiteral("not-authorized");
    }
    return std::nullopt;
}
