// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include <optional>

// Whether an account's password is locked, read from the shadow database,
// which only root may read. A child has no password: the login screen lets
// an L1 or L2 child in without one, and the lock keeps su, ssh and the lock
// screen out (#35). Nothing when the entry cannot be read.
namespace PasswordLock {

std::optional<bool> isLocked(const QString& login);

} // namespace PasswordLock
