// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// One run of cairn-family (ADR-0039) through pkexec, from the Guardian's
// window. pkexec asks for the Guardian's password or fingerprint, then
// runs the command as root, which makes one change and answers in one line.
//
// polkit judges the process that started pkexec, so pkexec is started
// directly, never through a shell. One change runs at a time.
class FamilyChange : public QObject {
    Q_OBJECT

public:
    FamilyChange(QString pkexec, QString command, QObject* parent = nullptr);

    bool running() const;
    // Starts the command with these arguments. False, and nothing starts,
    // while a change is still running. When the command is not there, it
    // answers "unknown" without running pkexec. Any `input`, such as a new
    // password, goes to the command's standard input, never into its
    // arguments, and the input then ends.
    bool start(const QStringList& arguments, const QByteArray& input = {});

signals:
    // "done"; the command's own word after "refused", such as "taken";
    // pkexec's "dismissed" or "not-authorized"; or "unknown" when there was
    // no answer the window can read.
    void finished(const QString& answer);

private:
    void ended(int exitCode, QProcess::ExitStatus status);
    void notStarted(QProcess::ProcessError error);
    void finish(const QString& answer);

    QString m_pkexec;
    QString m_command;
    QProcess* m_process = nullptr;
    // The command was not there; its answer is on the way.
    bool m_commandMissing = false;
};
