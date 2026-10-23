// SPDX-License-Identifier: Apache-2.0
#include "LoginName.h"

namespace {

constexpr qsizetype longestLogin = 32;
constexpr int lastNumber = 9999;

bool isLowerLetter(QChar character) {
    return character >= QLatin1Char('a') && character <= QLatin1Char('z');
}

bool isDigit(QChar character) {
    return character >= QLatin1Char('0') && character <= QLatin1Char('9');
}

bool startsALogin(QChar character) {
    return isLowerLetter(character) || character == QLatin1Char('_');
}

bool continuesALogin(QChar character) {
    return startsALogin(character) || isDigit(character) || character == QLatin1Char('-');
}

} // namespace

// Decomposed, an accented letter is the plain letter followed by its accent,
// a mark of its own, which is then dropped.
std::optional<QString> loginFromName(const QString& name) {
    const QString decomposed = name.toLower().normalized(QString::NormalizationForm_D);
    QString login;
    for (const QChar character : decomposed) {
        if (login.size() == longestLogin) {
            break;
        }
        if (login.isEmpty() ? startsALogin(character) : continuesALogin(character)) {
            login.append(character);
        }
    }
    if (login.isEmpty()) {
        return std::nullopt;
    }
    return login;
}

std::optional<QString> freeLogin(const QString& login,
                                 const std::function<bool(const QString&)>& inUse) {
    if (!inUse(login)) {
        return login;
    }
    for (int number = 2; number <= lastNumber; ++number) {
        const QString suffix = QString::number(number);
        QString candidate = login.left(longestLogin - suffix.size()) + suffix;
        if (!inUse(candidate)) {
            return candidate;
        }
    }
    return std::nullopt;
}
