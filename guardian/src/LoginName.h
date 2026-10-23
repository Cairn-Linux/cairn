// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include <functional>
#include <optional>

// The account's login name, made from the name a parent types for a child,
// so the parent never has to invent one. Only the computer uses it: the
// login screen shows the name the parent typed.
//
// It follows cairn-family's rule, ^[a-z_][a-z0-9_-]{0,31}$: lower case,
// accents dropped ("Zoë" is "zoe"), and only a to z, digits, - and _ kept,
// starting with a letter or _. Nothing when no such character is left, as
// for a name written only in another alphabet.
std::optional<QString> loginFromName(const QString& name);

// The login itself when no account or group has it, otherwise the first of
// login2, login3 ... that is free, cut short to stay within 32 characters.
// Nothing if every one up to login9999 is in use.
std::optional<QString> freeLogin(const QString& login,
                                 const std::function<bool(const QString&)>& inUse);
