// SPDX-License-Identifier: Apache-2.0
#include "LogOutCommand.h"

#include <QRegularExpression>

#include <utility>

namespace {

// The same rule as FamilyRequest's, GamesRequest's and the visit helper's.
bool isPlainLogin(const QString& login) {
    static const QRegularExpression plain(QStringLiteral("^[a-z_][a-z0-9_-]{0,31}$"));
    return plain.match(login).hasMatch();
}

// The helper's refusals the window can say something about. Any other
// means a mistake, and is held as `failed`.
bool isHoldingRefusal(const QString& word) {
    static const QStringList words{
        QStringLiteral("stuck"),      QStringLiteral("logged-in"), QStringLiteral("stopped"),
        QStringLiteral("busy"),       QStringLiteral("copying"),   QStringLiteral("logging-out"),
        QStringLiteral("not-set-up"),
    };
    return words.contains(word);
}

} // namespace

LogOutCommand::LogOutCommand(Setup setup) : m_setup(std::move(setup)) {}

LogOutCommand::Outcome LogOutCommand::run(const QStringList& arguments, const Say& say) const {
    const Caller caller =
        Caller::check(m_setup.euid, m_setup.pkexecUid, m_setup.lookUpUid, m_setup.lookUpHome);
    if (caller.refusal) {
        return {.refusal = caller.refusal, .asked = {}, .ended = {}};
    }
    if (arguments.size() != 2 || arguments.constFirst() != QStringLiteral("log-out")) {
        return {.refusal = Refusal::Usage, .asked = {}, .ended = {}};
    }
    const QString& login = arguments.at(1);
    if (!isPlainLogin(login)) {
        return {.refusal = Refusal::BadName, .asked = {}, .ended = {}};
    }
    const QString asked = QStringLiteral("log-out ") + login;
    const std::optional<Account> child = m_setup.lookUp(login);
    if (!child || !child->isCairnChild()) {
        return {.refusal = Refusal::NotAChild, .asked = asked, .ended = {}};
    }

    static const QRegularExpression refusal(QStringLiteral("^refused ([a-z-]{1,32})$"));
    std::optional<QString> held;
    bool finished = false;
    const int lines = SystemJob::run(
        {.systemdRun = m_setup.systemdRun,
         .unit = QStringLiteral("cairn-log-out-%1").arg(child->uid),
         .description = QStringLiteral("Cairn logs a child out for a grown-up"),
         .command = {m_setup.visitHelper, QStringLiteral("--log-out"), login}},
        [&](const QString& line) {
            if (finished || held) {
                return;
            }
            if (line == QStringLiteral("logging-out") || line == QStringLiteral("logged-out")) {
                say(line + QLatin1Char(' ') + login);
            } else if (line == QStringLiteral("finished")) {
                finished = true;
            } else if (const QRegularExpressionMatch match = refusal.match(line);
                       match.hasMatch()) {
                held = isHoldingRefusal(match.captured(1)) ? match.captured(1)
                                                           : QStringLiteral("failed");
            }
        });
    if (!finished && !held) {
        held = lines == 0 ? QStringLiteral("not-started") : QStringLiteral("failed");
    }
    if (held) {
        const QString line = QStringLiteral("held %1 %2").arg(login, *held);
        say(line);
        return {.refusal = std::nullopt, .asked = asked, .ended = line};
    }
    return {
        .refusal = std::nullopt, .asked = asked, .ended = QStringLiteral("logged-out ") + login};
}
