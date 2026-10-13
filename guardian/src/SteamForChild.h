// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "OwnSteam.h"

#include <QObject>
#include <QQmlEngine>
#include <QString>

#include <cstdint>

class ScreenLockWatch;
class SteamVisit;

// What the Guardian's window for one child's Steam shows, and what its
// buttons do (ADR-0032). It closes the Guardian's own Steam, opens the
// child's, closes the child's on Done, on the screen lock or when the
// window is closed, and then starts the Guardian's own Steam again.
class SteamForChild : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The window is given one by main().")
    Q_PROPERTY(QString childName READ childName CONSTANT)
    Q_PROPERTY(Step step READ step NOTIFY stepChanged)
    Q_PROPERTY(Reason reason READ reason NOTIFY stepChanged)
    Q_PROPERTY(bool ownSteamOpen READ ownSteamOpen NOTIFY ownSteamChanged)
    Q_PROPERTY(bool ownSteamComingBack READ ownSteamComingBack NOTIFY ownSteamChanged)

public:
    enum class Step : std::uint8_t {
        Ready,
        ClosingOwnSteam,
        Asking,
        Waiting,
        Open,
        Closing,
        Closed,
        Stuck,
        NotOpened,
    };
    Q_ENUM(Step)

    // Why the child's Steam did not open, in the words the window uses.
    enum class Reason : std::uint8_t {
        None,
        Busy,
        LoggedIn,
        StillEnding,
        NotSetUp,
        NotAChild,
        NotAtTheComputer,
        NoPassword,
        NotAllowed,
        OwnSteamStillOpen,
        Other,
    };
    Q_ENUM(Reason)

    struct Setup {
        QString child;
        QString pkexec = QStringLiteral("/usr/bin/pkexec");
        QString helper = QStringLiteral("/usr/local/libexec/cairn/cairn-steam-visit");
        uint uid = 0;
        OwnSteam::Programs ownSteam;
    };

    explicit SteamForChild(const Setup& setup, QObject* parent = nullptr);

    QString childName() const;
    Step step() const;
    Reason reason() const;
    bool ownSteamOpen() const;
    bool ownSteamComingBack() const;

    Q_INVOKABLE void open();
    Q_INVOKABLE void done();
    // The window's close button. True if the window may close now. If not,
    // whatever is under way is closed first and quitRequested() follows.
    Q_INVOKABLE bool leave();

    // The Guardian's screen locked.
    void screenLocked();

    static Reason reasonFor(const QString& refusal);

signals:
    void stepChanged();
    void ownSteamChanged();
    void quitRequested();

private:
    void ownSteamPaused(bool closed);
    void visitChanged();
    void finish();
    void setStep(Step step, Reason reason = Reason::None);

    QString m_child;
    QString m_childName;
    SteamVisit* m_visit;
    OwnSteam* m_ownSteam;
    ScreenLockWatch* m_lock;
    Step m_step = Step::Ready;
    Reason m_reason = Reason::None;
    bool m_ownSteamOpen = false;
    bool m_ownSteamComingBack = false;
    // The window was closed, or the screen locked, before the child's Steam
    // opened: it does not open after all, and the window starts over.
    bool m_stopBeforeOpening = false;
    bool m_quitWhenFinished = false;
};
