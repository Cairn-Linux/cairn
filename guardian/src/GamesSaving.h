// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ChildList.h"
#include "GameShelf.h"
#include "SaveReport.h"

#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <cstdint>
#include <vector>

class SaveRun;

// A Save on the Games page, as the page shows it (ADR-0041): the password
// box, how far cairn-family save-games has got, then what it did for each
// child, or why nothing was saved. The page gives it the arguments; this
// runs them and turns every word the command says into one of the values
// below, for the page to put into a sentence.
class GamesSaving : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("The Games page has one.")
    Q_PROPERTY(Step step READ step NOTIFY changed)
    Q_PROPERTY(Kind kind READ kind NOTIFY changed)
    Q_PROPERTY(Reason reason READ reason NOTIFY changed)
    Q_PROPERTY(Progress progress READ progress NOTIFY changed)
    // The name of the child being copied or waited for, or "".
    Q_PROPERTY(QString progressName READ progressName NOTIFY changed)
    // Each game copied so far, or found already there, as a map: login,
    // name, appId, title (or "") and had, true for a game already there.
    // For the progress window while the copies run.
    Q_PROPERTY(QVariantList copiedSoFar READ copiedSoFar NOTIFY changed)
    // Each child the command answered for, as a map: login, name, added,
    // removed, held (a Held), and skipped, a list of maps with appId,
    // title and reason (a Skip). added and removed are -1 when the child's
    // list was not written.
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    // No child the command answered for was held or had a game left out.
    Q_PROPERTY(bool allGiven READ allGiven NOTIFY changed)
    // The full games list stays as it was before the Save.
    Q_PROPERTY(bool catalogueKept READ catalogueKept NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)

public:
    // Names in QML are one namespace for every enum here, so none repeats.
    enum class Step : std::uint8_t { Ready, Asking, Saving, Saved, NotSaved };
    Q_ENUM(Step)
    // A Save of changed lists, or a look for new games that changes none.
    enum class Kind : std::uint8_t { Save, Look };
    Q_ENUM(Kind)
    // Why nothing was saved: cairn-family's refusal words, and pkexec's.
    enum class Reason : std::uint8_t {
        NoReason,
        NotRoot,
        NotAGrownUp,
        Usage, // also bad-name and bad-appid, which the page never sends
        NotAChild,
        AnotherSave, // busy
        Dismissed,
        NotAuthorized,
        // The command, or pkexec, could not be started, so nothing ran.
        CommandNotStarted,
        Unknown,
    };
    Q_ENUM(Reason)
    enum class Progress : std::uint8_t {
        Starting,
        Looking,
        Writing,
        CopyingGames,
        WaitingForLogout,
    };
    Q_ENUM(Progress)
    // A child's held word (ADR-0041).
    enum class Held : std::uint8_t {
        NotHeld,
        LoggedIn,
        VisitOpen,        // busy
        CopyStillRunning, // copying
        ChildLoggingOut,  // logging-out
        StillRunning,
        Stopped,
        NotSetUp,
        NoLibrary,
        NoShared,
        ListsShared,
        UnreadableConfig,
        NoReflink,
        CopyStopped, // failed
        NotStarted,
        NotWritten,
        OtherHold,
    };
    Q_ENUM(Held)
    // Why a game was left out, from cairn-family or the copy program.
    enum class Skip : std::uint8_t {
        NotShared,
        NoCatalogue,
        UnreadableManifest, // unreadable
        OtherGame,          // appid
        OddFolder,          // installdir
        BetaBranch,
        NotInstalled,
        Downloading,
        NoFolder,
        FolderThere, // folder-without-manifest
        ChangedDuringCopy,
        CopyDidNotWork, // copy-failed
        UnreadableFiles,
        OtherSkip,
    };
    Q_ENUM(Skip)

    GamesSaving(const QString& pkexec, const QString& command, QObject* parent = nullptr);

    Step step() const;
    Kind kind() const;
    Reason reason() const;
    Progress progress() const;
    QString progressName() const;
    QVariantList copiedSoFar() const;
    QVariantList results() const;
    bool allGiven() const;
    bool catalogueKept() const;
    bool busy() const;

    // Starts cairn-family with these arguments, "save-games" first, once
    // they pass the command's own checks; otherwise nothing is asked and
    // the step says why. The results name each child and game as
    // `children` and `games` do. False while a Save runs.
    bool start(Kind kind, const QStringList& arguments, const std::vector<ListedChild>& children,
               const QList<GameShelf::Game>& games);

    // The last Save's outcome stops showing, because the page has changed
    // since it ended. Nothing while a Save runs.
    void clearOutcome();
    // The progress window's Close: the same, once the Save has ended.
    Q_INVOKABLE void close();

    static Reason reasonFor(const QString& answer);

signals:
    void changed();
    // The Save has ended, however it ended, and the lists can be read again.
    void ended();

private:
    void lineRead(const QString& line);
    void finished(const QString& answer);
    QString nameOf(const QString& login) const;

    SaveRun* m_run;
    SaveReport m_report;
    Step m_step = Step::Ready;
    Kind m_kind = Kind::Save;
    Reason m_reason = Reason::NoReason;
    QHash<QString, QString> m_names;
    QHash<QString, QString> m_titles;
};
