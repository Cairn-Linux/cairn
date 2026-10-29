// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include <cstdint>

// Why cairn-family made no change. Each has one machine word, which the
// command prints as `refused WORD` and the Guardian's window turns into a
// sentence (ADR-0039).
enum class Refusal : std::uint8_t {
    // Not running as root.
    NotRoot,
    // Started through pkexec by an account that is not an administrator.
    NotAGrownUp,
    // Not a subcommand and arguments this command takes.
    Usage,
    // The login name is not a plain one.
    BadName,
    // The full name holds a character the account database or a menu entry
    // cannot carry.
    BadFullName,
    // Not a level a parent may give: only 1 and 2 have a child session.
    BadLevel,
    // The name belongs to an account Cairn did not make.
    Taken,
    // No child account has that name.
    NotAChild,
    // A system tool, or writing the menu entry, failed.
    Failed,
    // save-games: an appid that is not a plain number (ADR-0041).
    BadAppId,
    // save-games: another Save is still running.
    Busy,
    // finish-games: started through pkexec. Only systemd starts it, at a
    // child's logout (ADR-0041).
    NotForPkexec,
};

QString refusalWord(Refusal refusal);
