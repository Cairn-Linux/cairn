// SPDX-License-Identifier: Apache-2.0
#include "Account.h"

#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <vector>

namespace {

// Enough for any line of /etc/passwd or /etc/group on a family computer.
constexpr std::size_t lookUpBufferSize = 16384;

QString groupName(gid_t gid) {
    group entry{};
    group* found = nullptr;
    std::vector<char> buffer(lookUpBufferSize);
    if (getgrgid_r(gid, &entry, buffer.data(), buffer.size(), &found) != 0 || found == nullptr) {
        return {};
    }
    return QString::fromLocal8Bit(found->gr_name);
}

} // namespace

std::optional<Account> lookUpAccount(const QString& name) {
    const QByteArray login = name.toLocal8Bit();
    passwd entry{};
    passwd* found = nullptr;
    std::vector<char> buffer(lookUpBufferSize);
    if (login.isEmpty() ||
        getpwnam_r(login.constData(), &entry, buffer.data(), buffer.size(), &found) != 0 ||
        found == nullptr) {
        return std::nullopt;
    }

    // getgrouplist says how many groups there are when the first guess is too
    // small, so it runs at most twice.
    int count = 32;
    std::vector<gid_t> gids(static_cast<std::size_t>(count));
    if (getgrouplist(login.constData(), found->pw_gid, gids.data(), &count) < 0) {
        gids.resize(static_cast<std::size_t>(count));
        if (getgrouplist(login.constData(), found->pw_gid, gids.data(), &count) < 0) {
            return std::nullopt;
        }
    }
    gids.resize(static_cast<std::size_t>(count));

    Account account;
    account.uid = found->pw_uid;
    for (const gid_t gid : gids) {
        const QString group = groupName(gid);
        if (!group.isEmpty()) {
            account.groups.append(group);
        }
    }
    return account;
}
