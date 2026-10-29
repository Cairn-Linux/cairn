// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// One Save of the Games page (ADR-0041): cairn-family save-games through
// pkexec, which asks for one password for the whole Save. Unlike the
// Children page's changes, the command answers in many lines as it goes,
// so each is passed on as it arrives, and the last one is the answer.
//
// polkit judges the process that started pkexec, so pkexec is started
// directly, never through a shell. One Save runs at a time.
class SaveRun : public QObject {
    Q_OBJECT

public:
    SaveRun(QString pkexec, QString command, QObject* parent = nullptr);

    bool running() const;
    // Starts the command with these arguments, "save-games" first. False,
    // and nothing starts, while a Save is still running. When the command
    // is not there, it answers "unknown" without running pkexec.
    bool start(const QStringList& arguments);

signals:
    // Each whole line the command says, in order, the last one too. A line
    // longer than any the command says is dropped.
    void lineRead(const QString& line);
    // "done"; the command's own word after "refused", such as "busy";
    // pkexec's "dismissed" or "not-authorized"; or "unknown" when there was
    // no answer the window can read.
    void finished(const QString& answer);

private:
    void readLines();
    void take(const QByteArray& line);
    void ended(int exitCode, QProcess::ExitStatus status);
    void notStarted(QProcess::ProcessError error);
    void finish(const QString& answer);

    QString m_pkexec;
    QString m_command;
    QProcess* m_process = nullptr;
    // The command was not there; its answer is on the way.
    bool m_commandMissing = false;
    QString m_lastLine;
    int m_lines = 0;
    // Part of a line too long to be the command's was read, and the rest
    // of it, up to its end, is dropped too.
    bool m_inLongLine = false;
};
