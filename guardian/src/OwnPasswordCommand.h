// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Caller.h"
#include "Refusal.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

// cairn-family set-own-password: a grown-up changes their own password
// from the Family window (ADR-0043). pkexec has asked for the password
// they have now; this command reads the new one from its standard input,
// one line, checks it with NewPassword's rule and hands it to chpasswd on
// chpasswd's standard input. The password is never an argument, never
// written to the journal and never printed.
//
// Only the account pkexec names is changed: there is no login argument,
// so a grown-up can change no one's password but their own here. Under
// sudo no grown-up is named, and it is refused with `not-a-grown-up`.
class OwnPasswordCommand {
public:
    using ReadInput = std::function<std::optional<QByteArray>(qint64 maxBytes)>;
    using RunTool = std::function<bool(const QString& program, const QStringList& arguments,
                                       const QByteArray& input)>;

    // Everything the command reads from or does to the machine. main() keeps
    // the defaults; a test gives stand-ins.
    struct Setup {
        unsigned int euid = Caller::effectiveUid();
        std::optional<QString> pkexecUid = Caller::pkexecUidFromEnvironment();
        Caller::LookUpUid lookUpUid = lookUpAccountByUid;
        Caller::LookUpHome lookUpHome = ::lookUpHome;
        ReadInput readInput = readStandardInput;
        RunTool runTool = defaultRunTool;
        QString chpasswd = QStringLiteral("/usr/sbin/chpasswd");
    };

    struct Outcome {
        std::optional<Refusal> refusal;
        // For the journal: the subcommand and whose password, never the
        // password.
        QString asked;
    };

    explicit OwnPasswordCommand(Setup setup);

    Outcome run(const QStringList& arguments) const;

    // At most `maxBytes` and one more from standard input, to its end, so a
    // longer input is seen to be too long. Nothing when it cannot be read.
    static std::optional<QByteArray> readStandardInput(qint64 maxBytes);

private:
    static bool defaultRunTool(const QString& program, const QStringList& arguments,
                               const QByteArray& input);

    Setup m_setup;
};
