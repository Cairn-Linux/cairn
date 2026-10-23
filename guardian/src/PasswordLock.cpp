// SPDX-License-Identifier: Apache-2.0
#include "PasswordLock.h"

#include <shadow.h>
#include <vector>

namespace {

// Enough for any line of /etc/shadow on a family computer.
constexpr std::size_t lookUpBufferSize = 16384;

} // namespace

std::optional<bool> PasswordLock::isLocked(const QString& login) {
    const QByteArray name = login.toLocal8Bit();
    spwd entry{};
    spwd* found = nullptr;
    std::vector<char> buffer(lookUpBufferSize);
    if (name.isEmpty() ||
        getspnam_r(name.constData(), &entry, buffer.data(), buffer.size(), &found) != 0 ||
        found == nullptr || found->sp_pwdp == nullptr) {
        return std::nullopt;
    }
    // passwd --lock puts a "!" before the hash, and useradd gives a new
    // account "!!", so no password can match either.
    return found->sp_pwdp[0] == '!';
}
