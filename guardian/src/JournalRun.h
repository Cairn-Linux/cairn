// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

// One run of journalctl, read line by line while it runs, so the window
// never waits and no amount of output is too much (ADR-0054). A child can
// make thousands of real tries; they must make the page longer, never make
// it fail. A line longer than any entry the page reads is skipped whole.
//
// journalctl is named by its full path and started directly, with a small
// environment of its own, as SystemTool starts the system's tools.
class JournalRun : public QObject {
    Q_OBJECT

public:
    // The longest line kept, and how long a run may take.
    static constexpr qsizetype maxLine = 65536;
    static constexpr int maxMilliseconds = 120000;

    JournalRun(QString program, QStringList arguments, QObject* parent = nullptr);
    // A run still going when the window closes is stopped, and waited for.
    ~JournalRun() override;

    void start();

signals:
    void line(const QByteArray& line);
    // Whether journalctl started, ended by itself within the time, and
    // said it succeeded. Every line it printed has been given first.
    void finished(bool succeeded);

private:
    void readLines();
    void ended(int exitCode, QProcess::ExitStatus status);
    void notStarted(QProcess::ProcessError error);
    void finish(bool succeeded);

    QString m_program;
    QStringList m_arguments;
    QProcess* m_process = nullptr;
    QTimer m_limit;
    QByteArray m_partial;
    // Inside a line that was too long, until its end.
    bool m_skipping = false;
    bool m_timedOut = false;
};
