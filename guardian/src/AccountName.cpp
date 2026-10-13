// SPDX-License-Identifier: Apache-2.0
#include "AccountName.h"

#include <pwd.h>
#include <vector>

namespace {

// Enough for any line of /etc/passwd on a family computer.
constexpr std::size_t lookUpBufferSize = 16384;

} // namespace

QString AccountName::of(const QString& login) {
    const QByteArray name = login.toLocal8Bit();
    passwd entry{};
    passwd* found = nullptr;
    std::vector<char> buffer(lookUpBufferSize);
    if (name.isEmpty() ||
        getpwnam_r(name.constData(), &entry, buffer.data(), buffer.size(), &found) != 0 ||
        found == nullptr || found->pw_gecos == nullptr) {
        return login;
    }
    // The full name is the first of the comma-separated fields there.
    const QString fullName =
        QString::fromLocal8Bit(found->pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
    return fullName.isEmpty() ? login : fullName;
}
