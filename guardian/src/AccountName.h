// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

namespace AccountName {

// The name the family knows an account by: the full name on it, or the
// login name when it has none, as on the login screen.
QString of(const QString& login);

} // namespace AccountName
