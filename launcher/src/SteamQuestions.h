// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "SteamLaunchLog.h"

#include <QHash>
#include <QString>

#include <cstdint>

// The games Steam holds behind a question it asked on a window the kiosk
// hides, so a later tap on one says why it did not start too (#211).
//
// Offline, Steam asks what to do about a game whose Steam Cloud saves it
// could not check, and waits for an answer nobody in a child's session can
// give (ADR-0030, ADR-0036). On the test VM (2026-10-31) it kept that
// question for the game. Each later tap on it, even once Steam had signed
// in again, made Steam write only the tap's own line, until the child
// logged out and back in. Other games still started. So a question is
// remembered for its game, and a later tap that Steam takes on that game
// without writing a step is the same question. Steam writes that same
// single line for an app id it will not launch at all, so a tap with no
// step means a question only for a game Steam asked one about.
//
// The cloud question is told apart from Steam's other questions: logging
// the child out was seen to free a game it holds (ADR-0045).
class SteamQuestions {
public:
    // A tile was pressed. The app id is empty for anything but a Steam game.
    void startLaunch(const QString& appId);
    // Where the launch has got to, from Steam's launch log as just read: its
    // step, or WaitingForAnswer when Steam took the tap on a game it still
    // holds behind a question and has written no step for it since.
    SteamLaunchLog::Step read(const SteamLaunchLog& launch);
    // At the last read(), Steam had taken the tap on a game it still holds
    // behind the cloud question from an earlier tap, and had written no
    // step for it since. Never at the tap that met the question.
    bool heldByCloudQuestion() const;
    // Steam was started again: its questions went with the client that
    // asked them.
    void steamRestarted();

private:
    // Which question Steam asked about a game.
    enum class Question : std::uint8_t {
        // What to do about the game's Steam Cloud saves, which it could not
        // check.
        Cloud,
        // Any other question.
        Other,
    };

    QString m_appId;
    // The games Steam asked a question about and has written no other step
    // for since, each with its question. Forgotten once Steam writes another
    // step for one, or a new client starts.
    QHash<QString, Question> m_questioned;
    bool m_heldByCloudQuestion = false;
};
