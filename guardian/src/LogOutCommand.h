// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "Account.h"
#include "Caller.h"
#include "Refusal.h"
#include "SystemJob.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

// cairn-family log-out LOGIN: the root side of the Games page's "Log ada
// out" (ADR-0041). It checks the caller and the child, then runs the visit
// helper's `--log-out CHILD` as a system job, so a Guardian who logs out
// meanwhile does not stop it halfway. It never copies anything.
//
// It passes on `logging-out LOGIN` and `logged-out LOGIN`, with the login
// from its own argument, never from the helper. A logout that did not
// finish is one line `held LOGIN WORD`: `stuck`, `logged-in`, `stopped`,
// `busy`, `copying` or `not-set-up` as the helper said it, `not-started`
// when the job never started, or `failed`. Then it ends with `done`.
// `refused WORD` means it was not tried at all.
class LogOutCommand {
public:
    using Say = std::function<void(const QString&)>;
    using LookUp = std::function<std::optional<Account>(const QString&)>;

    // Everything the command reads from or does to the machine. main() keeps
    // the defaults and gives the helper's path; a test gives stand-ins.
    struct Setup {
        unsigned int euid = Caller::effectiveUid();
        std::optional<QString> pkexecUid = Caller::pkexecUidFromEnvironment();
        LookUp lookUp = lookUpAccount;
        Caller::LookUpUid lookUpUid = lookUpAccountByUid;
        Caller::LookUpHome lookUpHome = ::lookUpHome;
        QString systemdRun = QStringLiteral("/usr/bin/systemd-run");
        QString visitHelper;
    };

    struct Outcome {
        std::optional<Refusal> refusal;
        // For the journal, only values already checked: what was asked, and
        // how the logout ended.
        QString asked;
        QString ended;
    };

    explicit LogOutCommand(Setup setup);

    Outcome run(const QStringList& arguments, const Say& say) const;

private:
    Setup m_setup;
};
