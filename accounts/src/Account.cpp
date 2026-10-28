// SPDX-License-Identifier: Apache-2.0
#include "Account.h"

#include "FamilyRole.h"

#include <cerrno>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#include <utility>

namespace {

// Enough for any line of /etc/passwd or /etc/group on a family computer.
constexpr std::size_t lookUpBufferSize = 16384;
constexpr std::size_t largestEntry = 1048576;

QString groupName(gid_t gid) {
    group entry{};
    group* found = nullptr;
    std::vector<char> buffer(lookUpBufferSize);
    if (getgrgid_r(gid, &entry, buffer.data(), buffer.size(), &found) != 0 || found == nullptr) {
        return {};
    }
    return QString::fromLocal8Bit(found->gr_name);
}

std::optional<Account> fromEntry(const passwd& found) {
    // getgrouplist says how many groups there are when the first guess is too
    // small, so it runs at most twice.
    int count = 32;
    std::vector<gid_t> gids(static_cast<std::size_t>(count));
    if (getgrouplist(found.pw_name, found.pw_gid, gids.data(), &count) < 0) {
        gids.resize(static_cast<std::size_t>(count));
        if (getgrouplist(found.pw_name, found.pw_gid, gids.data(), &count) < 0) {
            return std::nullopt;
        }
    }
    gids.resize(static_cast<std::size_t>(count));

    Account account;
    account.uid = found.pw_uid;
    account.login = QString::fromLocal8Bit(found.pw_name);
    for (const gid_t gid : gids) {
        const QString group = groupName(gid);
        if (!group.isEmpty()) {
            account.groups.append(group);
        }
    }
    // The full name is the first of the comma-separated fields there.
    if (found.pw_gecos != nullptr) {
        account.fullName =
            QString::fromLocal8Bit(found.pw_gecos).section(QLatin1Char(','), 0, 0).trimmed();
    }
    return account;
}

} // namespace

QString Account::displayName() const {
    return fullName.isEmpty() ? login : fullName;
}

bool Account::isCairnChild() const {
    return uid >= firstPersonUid && FamilyRole::fromGroups(groups).kind == FamilyRole::Kind::Child;
}

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
    return fromEntry(*found);
}

std::optional<Account> lookUpAccountByUid(unsigned int uid) {
    passwd entry{};
    passwd* found = nullptr;
    std::vector<char> buffer(lookUpBufferSize);
    if (getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &found) != 0 || found == nullptr) {
        return std::nullopt;
    }
    return fromEntry(*found);
}

std::optional<QString> lookUpHome(unsigned int uid) {
    passwd entry{};
    passwd* found = nullptr;
    std::vector<char> buffer(lookUpBufferSize);
    if (getpwuid_r(uid, &entry, buffer.data(), buffer.size(), &found) != 0 || found == nullptr ||
        found->pw_dir == nullptr || found->pw_dir[0] != '/') {
        return std::nullopt;
    }
    return QString::fromLocal8Bit(found->pw_dir);
}

// getpwent_r says ERANGE when an entry does not fit the buffer, and then the
// same entry comes again with a bigger one, up to a size no real entry needs.
std::vector<Account> listAccounts() {
    std::vector<Account> accounts;
    std::vector<char> buffer(lookUpBufferSize);
    passwd entry{};
    passwd* found = nullptr;
    setpwent();
    for (;;) {
        const int result = getpwent_r(&entry, buffer.data(), buffer.size(), &found);
        if (result == ERANGE && buffer.size() < largestEntry) {
            buffer.resize(buffer.size() * 2);
            continue;
        }
        if (result != 0 || found == nullptr) {
            break;
        }
        if (std::optional<Account> account = fromEntry(*found)) {
            accounts.push_back(std::move(*account));
        }
    }
    endpwent();
    return accounts;
}

bool nameInUse(const QString& name) {
    const QByteArray bytes = name.toLocal8Bit();
    group entry{};
    group* found = nullptr;
    std::vector<char> buffer(lookUpBufferSize);
    const bool isGroup =
        !bytes.isEmpty() &&
        getgrnam_r(bytes.constData(), &entry, buffer.data(), buffer.size(), &found) == 0 &&
        found != nullptr;
    return isGroup || lookUpAccount(name).has_value();
}
