// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <QtGlobal>

// Milliseconds since the computer started, counting time asleep. The launcher
// and cairn-give-up both use it (ADR-0053): the helper reads the same clock
// from /proc/uptime. Unlike the wall clock it never steps back, so a clock set
// by the network between a tap and the give-up key cannot change which launch
// came first.
namespace BootClock {
qint64 milliseconds();
} // namespace BootClock
