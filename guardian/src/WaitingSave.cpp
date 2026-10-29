// SPDX-License-Identifier: Apache-2.0
#include "WaitingSave.h"

#include "GamesRequest.h"
#include "RootFolder.h"

#include <utility>

namespace {

WaitingSave::Read refused() {
    return {.found = WaitingSave::Found::Refused, .appIds = {}};
}

} // namespace

WaitingSave::WaitingSave(QString folder, unsigned int owner, QString restorecon)
    : m_folder(std::move(folder)), m_owner(owner), m_restorecon(std::move(restorecon)) {}

WaitingSave::Read WaitingSave::read(const Account& child) const {
    if (!RootFolder::holds(m_folder, child.login)) {
        return {};
    }
    const std::optional<RootFolder::File> file =
        RootFolder::read(m_folder, m_owner, child.login, largestFile);
    if (!file || !file->bytes.endsWith('\n') || file->bytes.count('\n') != 1) {
        return refused();
    }
    const QString line = QString::fromUtf8(file->bytes.chopped(1));
    const GamesRequest request = GamesRequest::fromArguments({QStringLiteral("save-games"), line});
    if (request.refusal || request.children.size() != 1 ||
        request.children.constFirst().login != child.login ||
        request.children.constFirst().appIds.isEmpty() || !child.isCairnChild()) {
        return refused();
    }
    return {.found = Found::Waiting, .appIds = request.children.constFirst().appIds};
}

bool WaitingSave::write(const QString& login, const QStringList& appIds) const {
    const QString line =
        login + QLatin1Char('=') + appIds.join(QLatin1Char(',')) + QLatin1Char('\n');
    return RootFolder::write(m_folder, m_owner, m_restorecon, login, line.toUtf8());
}

bool WaitingSave::remove(const QString& login) const {
    return RootFolder::remove(m_folder, m_owner, login);
}

// `stuck` comes from the visit helper's log-out, a later step of ADR-0041.
bool WaitingSave::keeps(const std::optional<QString>& held) {
    static const QStringList waitsForALogout{
        QStringLiteral("logged-in"),   QStringLiteral("still-running"), QStringLiteral("stopped"),
        QStringLiteral("not-started"), QStringLiteral("stuck"),
    };
    return held && waitsForALogout.contains(*held);
}
