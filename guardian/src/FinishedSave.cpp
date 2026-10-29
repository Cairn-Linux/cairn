// SPDX-License-Identifier: Apache-2.0
#include "FinishedSave.h"

#include "RootFolder.h"

#include <utility>

FinishedSave::FinishedSave(QString folder, unsigned int owner, QString restorecon)
    : m_folder(std::move(folder)), m_owner(owner), m_restorecon(std::move(restorecon)) {}

std::optional<FinishedSave::Finished> FinishedSave::read(const QString& login) const {
    const std::optional<RootFolder::File> file =
        RootFolder::read(m_folder, m_owner, login, largestFile);
    if (!file) {
        return std::nullopt;
    }
    return Finished{.lines =
                        QString::fromUtf8(file->bytes).split(QLatin1Char('\n'), Qt::SkipEmptyParts),
                    .when = file->modified};
}

// The old file goes first, so the new one carries the time of this copy
// even when it says the same.
bool FinishedSave::write(const QString& login, const QStringList& lines) const {
    remove(login);
    const QByteArray text = (lines.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
    return RootFolder::write(m_folder, m_owner, m_restorecon, login, text);
}

bool FinishedSave::remove(const QString& login) const {
    return RootFolder::remove(m_folder, m_owner, login);
}
