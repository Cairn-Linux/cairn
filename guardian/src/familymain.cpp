// SPDX-License-Identifier: Apache-2.0
#include "FamilyCommand.h"

#include <QCoreApplication>

#include <cstdio>
#include <cstdlib>
#include <syslog.h>

// cairn-family (ADR-0039). It answers on standard output in one line of
// machine words, `done` or `refused REASON`, for the Guardian's window and
// the family's setup to read. It writes one line to the journal as well, read
// with journalctl -t cairn-family, whoever started it.
int main(int argc, char* argv[]) {
    const QCoreApplication application(argc, argv);
    QStringList arguments = QCoreApplication::arguments();
    arguments.removeFirst();

    FamilyCommand::Setup setup;
    setup.applications = QStringLiteral(CAIRN_APPLICATIONS);
    setup.steamFor = QStringLiteral(CAIRN_STEAM_FOR);
    const FamilyCommand::Outcome outcome = FamilyCommand(setup).run(arguments);

    const QString answer = outcome.refusal
                               ? QStringLiteral("refused ") + refusalWord(*outcome.refusal)
                               : QStringLiteral("done");
    std::fprintf(stdout, "%s\n", answer.toUtf8().constData());
    std::fflush(stdout);
    const QString note =
        outcome.asked.isEmpty() ? answer : outcome.asked + QStringLiteral(": ") + answer;
    openlog("cairn-family", 0, LOG_AUTHPRIV);
    syslog(outcome.refusal ? LOG_NOTICE : LOG_INFO, "%s", note.toUtf8().constData());
    closelog();
    return outcome.refusal ? EXIT_FAILURE : EXIT_SUCCESS;
}
