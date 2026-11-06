// SPDX-License-Identifier: Apache-2.0
#include "BootClock.h"

#include <ctime>

namespace BootClock {

qint64 milliseconds() {
    timespec now{};
    // CLOCK_BOOTTIME is the clock behind /proc/uptime. It cannot fail with a
    // valid clock and pointer, so the result is not checked.
    clock_gettime(CLOCK_BOOTTIME, &now);
    return (static_cast<qint64>(now.tv_sec) * 1000) + (now.tv_nsec / 1000000);
}

} // namespace BootClock
