// SPDX-License-Identifier: Apache-2.0
#include "FamilyCommand.h"
#include "GamesSave.h"
#include "LogOutCommand.h"
#include "OwnPasswordCommand.h"

#include <QCoreApplication>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <syslog.h>

namespace {

void sayLine(const QString& line) {
    std::fprintf(stdout, "%s\n", line.toUtf8().constData());
    std::fflush(stdout);
}

void note(bool refused, const QString& text) {
    syslog(refused ? LOG_NOTICE : LOG_INFO, "%s", text.toUtf8().constData());
}

QString answerFor(const std::optional<Refusal>& refusal) {
    return refusal ? QStringLiteral("refused ") + refusalWord(*refusal) : QStringLiteral("done");
}

// A window closed mid-Save must not stop the Save between a child's copy
// and their games file: with SIGPIPE ignored, a line to a closed window is
// only lost.
int saveGames(const QStringList& arguments) {
    std::signal(SIGPIPE, SIG_IGN);
    GamesSave::Setup setup;
    setup.catalogue.pictureStep = QStringLiteral(CAIRN_GAME_PICTURES);
    setup.catalogue.kidscan = QStringLiteral(CAIRN_KIDSCAN);
    setup.tools.shareStep = QStringLiteral(CAIRN_SHARE_TOOLS);
    setup.copy.visitHelper = QStringLiteral(CAIRN_STEAM_VISIT);
    const GamesSave::Outcome outcome = GamesSave(setup).run(arguments, sayLine);

    const QString answer = answerFor(outcome.refusal);
    sayLine(answer);
    note(outcome.refusal.has_value(),
         outcome.asked.isEmpty() ? answer : outcome.asked + QStringLiteral(": ") + answer);
    for (const QString& line : outcome.pictures + outcome.catalogue + outcome.tools) {
        note(false, line);
    }
    for (const QString& child : outcome.children) {
        note(false, child);
    }
    return outcome.refusal ? EXIT_FAILURE : EXIT_SUCCESS;
}

// A grown-up logs a child out before choosing their games. The logout runs
// as a system job, so it goes on to its end even if this program does not.
int logOutChild(const QStringList& arguments) {
    std::signal(SIGPIPE, SIG_IGN);
    LogOutCommand::Setup setup;
    setup.visitHelper = QStringLiteral(CAIRN_STEAM_VISIT);
    const LogOutCommand::Outcome outcome = LogOutCommand(setup).run(arguments, sayLine);

    const QString answer = answerFor(outcome.refusal);
    sayLine(answer);
    note(outcome.refusal.has_value(),
         outcome.asked.isEmpty() ? answer : outcome.asked + QStringLiteral(": ") + answer);
    if (!outcome.ended.isEmpty()) {
        note(false, outcome.ended);
    }
    return outcome.refusal ? EXIT_FAILURE : EXIT_SUCCESS;
}

// A grown-up's own new password, read from standard input (ADR-0043). The
// journal gets whose password was asked for and the answer, never the
// password.
int setOwnPassword(const QStringList& arguments) {
    const OwnPasswordCommand::Outcome outcome =
        OwnPasswordCommand(OwnPasswordCommand::Setup{}).run(arguments);

    const QString answer = answerFor(outcome.refusal);
    sayLine(answer);
    note(outcome.refusal.has_value(),
         outcome.asked.isEmpty() ? answer : outcome.asked + QStringLiteral(": ") + answer);
    return outcome.refusal ? EXIT_FAILURE : EXIT_SUCCESS;
}

int changeAccount(const QStringList& arguments) {
    FamilyCommand::Setup setup;
    setup.applications = QStringLiteral(CAIRN_APPLICATIONS);
    setup.steamFor = QStringLiteral(CAIRN_STEAM_FOR);
    const FamilyCommand::Outcome outcome = FamilyCommand(setup).run(arguments);

    const QString answer = answerFor(outcome.refusal);
    sayLine(answer);
    note(outcome.refusal.has_value(),
         outcome.asked.isEmpty() ? answer : outcome.asked + QStringLiteral(": ") + answer);
    return outcome.refusal ? EXIT_FAILURE : EXIT_SUCCESS;
}

} // namespace

// cairn-family (ADR-0039, ADR-0041, ADR-0043). It answers on standard output in lines
// of machine words, ending with `done` or `refused REASON`, for the
// Guardian's window and the family's setup to read. It writes to the journal
// as well, read with journalctl -t cairn-family, whoever started it.
int main(int argc, char* argv[]) {
    const QCoreApplication application(argc, argv);
    QStringList arguments = QCoreApplication::arguments();
    arguments.removeFirst();

    openlog("cairn-family", 0, LOG_AUTHPRIV);
    const QString subcommand = arguments.value(0);
    int status = EXIT_FAILURE;
    if (subcommand == QStringLiteral("save-games")) {
        status = saveGames(arguments);
    } else if (subcommand == QStringLiteral("log-out")) {
        status = logOutChild(arguments);
    } else if (subcommand == QStringLiteral("set-own-password")) {
        status = setOwnPassword(arguments);
    } else {
        status = changeAccount(arguments);
    }
    closelog();
    return status;
}
