// SPDX-License-Identifier: Apache-2.0
#include "OwnPasswordCommand.h"

#include "NewPassword.h"
#include "SystemTool.h"

#include <QFile>
#include <QStringDecoder>

#include <cstdio>
#include <utility>

namespace {

// The longest password the rule takes, four bytes to a character in
// UTF-8, and its line break.
constexpr qint64 maxInputBytes = (NewPassword::maxLength * 4) + 1;

// The new password from the input: one line of UTF-8, with or without its
// line break. Nothing when it is not one line, not UTF-8, or not a
// password the rule takes.
std::optional<QString> passwordFrom(QByteArray input) {
    if (input.endsWith('\n')) {
        input.chop(1);
    }
    QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString password = utf8.decode(input);
    if (utf8.hasError() || NewPassword::check(password)) {
        return std::nullopt;
    }
    return password;
}

// Best effort only: a copy the system or Qt made is not reached.
void forget(QByteArray& secret) {
    secret.fill('\0');
    secret.clear();
}

} // namespace

OwnPasswordCommand::OwnPasswordCommand(Setup setup) : m_setup(std::move(setup)) {}

OwnPasswordCommand::Outcome OwnPasswordCommand::run(const QStringList& arguments) const {
    const Caller caller =
        Caller::check(m_setup.euid, m_setup.pkexecUid, m_setup.lookUpUid, m_setup.lookUpHome);
    if (caller.refusal) {
        return {.refusal = caller.refusal, .asked = {}};
    }
    if (arguments != QStringList{QStringLiteral("set-own-password")}) {
        return {.refusal = Refusal::Usage, .asked = {}};
    }
    if (!caller.login) {
        return {.refusal = Refusal::NotAGrownUp, .asked = {}};
    }
    const QString asked = QStringLiteral("set-own-password ") + *caller.login;

    std::optional<QByteArray> input = m_setup.readInput(maxInputBytes);
    if (!input || input->size() > maxInputBytes) {
        if (input) {
            forget(*input);
        }
        return {.refusal = Refusal::BadPassword, .asked = asked};
    }
    const std::optional<QString> password = passwordFrom(*input);
    forget(*input);
    if (!password) {
        return {.refusal = Refusal::BadPassword, .asked = asked};
    }

    // chpasswd takes LOGIN:PASSWORD, one to a line, and makes the hash
    // with the method login.defs names. A login name never holds a colon.
    QByteArray line = caller.login->toUtf8() + ':' + password->toUtf8() + '\n';
    const bool changed = m_setup.runTool(m_setup.chpasswd, {}, line);
    forget(line);
    if (!changed) {
        return {.refusal = Refusal::Failed, .asked = asked};
    }
    return {.refusal = std::nullopt, .asked = asked};
}

std::optional<QByteArray> OwnPasswordCommand::readStandardInput(qint64 maxBytes) {
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        return std::nullopt;
    }
    QByteArray read;
    while (read.size() <= maxBytes) {
        const QByteArray more = input.read(maxBytes + 1 - read.size());
        if (more.isEmpty()) {
            break;
        }
        read += more;
    }
    return read;
}

bool OwnPasswordCommand::defaultRunTool(const QString& program, const QStringList& arguments,
                                        const QByteArray& input) {
    return SystemTool::runWithInput(program, arguments, input);
}
