// SPDX-License-Identifier: Apache-2.0
#include "StandInSddm.h"

StandInSddm::StandInSddm(QObject* parent) : QObject(parent) {}

void StandInSddm::login(const QString& user, const QString& password, int sessionIndex) {
    Q_UNUSED(sessionIndex)
    ++m_logins;
    m_lastUser = user;
    m_lastPassword = password;
    emit loggedIn();
}

void StandInSddm::fail() {
    emit loginFailed();
}

int StandInSddm::logins() const {
    return m_logins;
}

QString StandInSddm::lastUser() const {
    return m_lastUser;
}

QString StandInSddm::lastPassword() const {
    return m_lastPassword;
}

StandInSessions::StandInSessions(QObject* parent) : QObject(parent) {}

int StandInSessions::lastIndex() const {
    return 0;
}
