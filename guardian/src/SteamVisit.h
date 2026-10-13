// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

#include <cstdint>

// One visit to a child's own Steam (ADR-0032), seen from the Guardian's side.
//
// The root helper, session/bin/cairn-steam-visit, does the work. It is
// started once per visit through pkexec, which asks for the Guardian's
// password or fingerprint, and that one root process lasts until the visit
// closes. It says how the visit is going one line at a time on its output,
// and closes the visit when it reads "done" or its input ends.
//
// polkit judges the process that started pkexec, so this object's process
// must stay alive for the whole visit: pkexec is started directly, never
// through a shell that could exit.
class SteamVisit : public QObject {
    Q_OBJECT

public:
    enum class State : std::uint8_t {
        NotStarted,
        Asking,  // pkexec is asking for the password
        Waiting, // the child's last login is still ending
        Open,
        Closing,
        Closed,
        Stuck,     // a program of the child's would not end
        NotOpened, // see refusal()
    };
    Q_ENUM(State)

    SteamVisit(QString pkexec, QString helper, QObject* parent = nullptr);
    ~SteamVisit() override;

    State state() const;
    // Why the visit did not open: the helper's own word ("busy",
    // "logged-in", ...), pkexec's "dismissed" or "not-authorized", or
    // "failed" when nothing said why.
    QString refusal() const;

    void open(const QString& child);
    // Asks the helper to close the visit. Before the password is given it
    // stops pkexec instead, so nothing opens.
    void close();

signals:
    void stateChanged();

private:
    void readLines();
    void takeLine(const QString& line);
    void ended(int exitCode, QProcess::ExitStatus status);
    void notStarted(QProcess::ProcessError error);
    void setState(State state, const QString& refusal = QString());

    QString m_pkexec;
    QString m_helper;
    QProcess* m_process = nullptr;
    State m_state = State::NotStarted;
    QString m_refusal;
};
