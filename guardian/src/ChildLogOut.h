// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <cstdint>

class SaveRun;

// "Log ada out" on the Games page (ADR-0041): a child who is logged in has
// their games greyed until they are logged out. First a warning that
// anything they have not saved is lost, then the password box, then
// cairn-family log-out through pkexec, which asks the child's Steam to
// save, ends their sessions, and kills what is left. It never copies any
// games; the parent chooses those once the child is out.
class ChildLogOut : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The Games page has one.")
    Q_PROPERTY(Step step READ step NOTIFY changed)
    Q_PROPERTY(Reason reason READ reason NOTIFY changed)
    // The child being logged out, or "" when none is.
    Q_PROPERTY(QString login READ login NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)

public:
    // Names in QML are one namespace for every enum here, so none repeats.
    enum class Step : std::uint8_t { NotAsking, Warning, Asking, LoggingOut, NotLoggedOut };
    Q_ENUM(Step)
    // Why the child is not logged out: the helper's held words, then
    // cairn-family's refusals and pkexec's.
    enum class Reason : std::uint8_t {
        NoReason,
        Stuck,
        StillClosing,  // logged-in
        StoppedEarly,  // stopped
        VisitOpen,     // busy
        CopyRunning,   // copying
        AnotherLogOut, // logging-out
        NotSetUp,
        NotStarted,
        Dismissed,
        NotAuthorized,
        NotAGrownUp,
        NotAChild,
        // The command, or pkexec, could not be started, so nothing ran.
        CommandNotStarted,
        Unknown,
    };
    Q_ENUM(Reason)

    ChildLogOut(const QString& pkexec, const QString& command, QObject* parent = nullptr);

    Step step() const;
    Reason reason() const;
    QString login() const;
    QString name() const;
    // The password box is up, or the logout runs.
    bool busy() const;
    // Whether the warning, the logout or its outcome is about this child.
    bool isAbout(const QString& login) const;

    // Shows the warning for this child. Nothing while a logout runs.
    void ask(const QString& login, const QString& name);
    // After the warning: asks for the password and logs the child out.
    Q_INVOKABLE void confirm();
    // After the warning: nothing happens.
    Q_INVOKABLE void notNow();
    // An outcome that said why the child is not logged out stops showing.
    Q_INVOKABLE void close();

signals:
    void changed();
    // The logout has ended, however it ended: who is logged in is read
    // again.
    void ended();

private:
    void lineRead(const QString& line);
    void finished(const QString& answer);
    void clear();

    SaveRun* m_run;
    Step m_step = Step::NotAsking;
    Reason m_reason = Reason::NoReason;
    QString m_login;
    QString m_name;
    // The held word the command said, if any.
    QString m_held;
};
