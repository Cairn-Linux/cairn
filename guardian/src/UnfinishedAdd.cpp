// SPDX-License-Identifier: Apache-2.0
#include "UnfinishedAdd.h"

void UnfinishedAdd::starting(const QString& login, const QString& fullName) {
    m_adding = Add{.login = login, .fullName = fullName};
}

// Remembered until an add with that login is done.
void UnfinishedAdd::answered(const QString& answer, bool readable) {
    if (answer == QLatin1String("done")) {
        if (m_unfinished && m_unfinished->login == m_adding.login) {
            m_unfinished.reset();
        }
    } else if (answer == QLatin1String("failed") || !readable) {
        m_unfinished = m_adding;
    }
}

std::optional<QString>
UnfinishedAdd::loginFor(const QString& fullName, const std::vector<ListedChild>& children,
                        const std::function<bool(const QString&)>& nameInUse) const {
    if (!m_unfinished || !sameName(m_unfinished->fullName, fullName)) {
        return std::nullopt;
    }
    const QString& earlier = m_unfinished->login;
    const ListedChild* child = childWithLogin(children, earlier);
    if ((child != nullptr && sameName(child->name, fullName)) || !nameInUse(earlier)) {
        return earlier;
    }
    return std::nullopt;
}
