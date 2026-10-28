// SPDX-License-Identifier: Apache-2.0
#include "FamilyCommand.h"
#include "GamesSave.h"

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
    setup.copy.visitHelper = QStringLiteral(CAIRN_STEAM_VISIT);
    const GamesSave::Outcome outcome = GamesSave(setup).run(arguments, sayLine);

    const QString answer = answerFor(outcome.refusal);
    sayLine(answer);
    note(outcome.refusal.has_value(),
         outcome.asked.isEmpty() ? answer : outcome.asked + QStringLiteral(": ") + answer);
    for (const QString& child : outcome.children) {
        note(false, child);
    }
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

// cairn-family (ADR-0039, ADR-0041). It answers on standard output in lines
// of machine words, ending with `done` or `refused REASON`, for the
// Guardian's window and the family's setup to read. It writes to the journal
// as well, read with journalctl -t cairn-family, whoever started it.
int main(int argc, char* argv[]) {
    const QCoreApplication application(argc, argv);
    QStringList arguments = QCoreApplication::arguments();
    arguments.removeFirst();

    openlog("cairn-family", 0, LOG_AUTHPRIV);
    const int status =
        !arguments.isEmpty() && arguments.constFirst() == QStringLiteral("save-games")
            ? saveGames(arguments)
            : changeAccount(arguments);
    closelog();
    return status;
}
