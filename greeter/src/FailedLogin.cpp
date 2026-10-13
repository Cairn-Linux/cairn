// SPDX-License-Identifier: Apache-2.0
#include "FailedLogin.h"

#include <QFile>

FailedLogin::FailedLogin(QObject* parent) : QObject(parent) {}

QUrl FailedLogin::visitsFile() const {
    return m_visitsFile;
}

void FailedLogin::setVisitsFile(const QUrl& file) {
    if (file == m_visitsFile) {
        return;
    }
    m_visitsFile = file;
    emit visitsFileChanged();
}

// The list comes first. PAM refuses a listed child whatever they type, and
// SDDM does not say which step said no, so a listed child is never told
// their password was wrong.
FailedLogin::Reason FailedLogin::reasonFor(const QString& loginName, bool gavePassword) const {
    if (lists(m_visitsFile.toLocalFile(), loginName)) {
        return Reason::SteamVisit;
    }
    return gavePassword ? Reason::WrongPassword : Reason::NeedsGrownUp;
}

bool FailedLogin::lists(const QString& file, const QString& loginName) {
    QFile list(file);
    if (loginName.isEmpty() || !list.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    while (!list.atEnd()) {
        if (QString::fromUtf8(list.readLine()).trimmed() == loginName) {
            return true;
        }
    }
    return false;
}
