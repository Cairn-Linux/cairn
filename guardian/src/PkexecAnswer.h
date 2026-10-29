// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QString>

#include <optional>

// What pkexec's own exit code means when it never ran the command, for the
// Guardian tool's runs of cairn-family (ADR-0039, ADR-0041).
namespace PkexecAnswer {

// "dismissed" for 126, the password box closed; "not-authorized" for 127,
// the password not accepted, which Plasma's polkit agent also gives for its
// Cancel button (VM, 2026-10-12). Nothing for any other code. The caller
// asks only when the command printed nothing, because cairn-family never
// exits with either.
std::optional<QString> fromExitCode(int exitCode);

} // namespace PkexecAnswer
