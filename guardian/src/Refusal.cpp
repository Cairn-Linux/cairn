// SPDX-License-Identifier: Apache-2.0
#include "Refusal.h"

QString refusalWord(Refusal refusal) {
    switch (refusal) {
    case Refusal::NotRoot:
        return QStringLiteral("not-root");
    case Refusal::NotAGrownUp:
        return QStringLiteral("not-a-grown-up");
    case Refusal::Usage:
        return QStringLiteral("usage");
    case Refusal::BadName:
        return QStringLiteral("bad-name");
    case Refusal::BadFullName:
        return QStringLiteral("bad-full-name");
    case Refusal::BadLevel:
        return QStringLiteral("bad-level");
    case Refusal::Taken:
        return QStringLiteral("taken");
    case Refusal::NotAChild:
        return QStringLiteral("not-a-child");
    case Refusal::Failed:
        return QStringLiteral("failed");
    case Refusal::BadAppId:
        return QStringLiteral("bad-appid");
    case Refusal::Busy:
        return QStringLiteral("busy");
    case Refusal::NotForPkexec:
        return QStringLiteral("not-for-pkexec");
    }
    return QStringLiteral("failed");
}
