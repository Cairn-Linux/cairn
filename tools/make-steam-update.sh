#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Makes up a Steam self-update for the launcher to see, on a dev PC or in
# a test VM, for a throwaway account or a throwaway HOME (ADR-0061). It
# writes the lines Steam's updater and client write while Steam updates
# itself into the Steam log folder the launcher reads, and runs a stand-in
# process called `steam` meanwhile. Then it writes the restart and the
# client's own start command and stops the stand-in. The game tiles fade
# and show building stones until then.
#
#   tools/make-steam-update.sh [--home DIR] [--seconds N]
#
#   --home DIR    write under DIR/.local/share/Steam/logs, for a launcher
#                 started with HOME=DIR (default: this account's own
#                 ${XDG_DATA_HOME:-$HOME/.local/share}/Steam/logs)
#   --seconds N   how long the update lasts (default 45); Enter ends it
#                 sooner
#
# It refuses a Steam folder that looks like a real install, so it never
# writes into a child's real Steam. It is never installed into the image.

set -ouex pipefail

usage() {
    sed -n '3,21p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

data_home=${XDG_DATA_HOME:-${HOME}/.local/share}
seconds=45
while [[ $# -gt 0 ]]; do
    case $1 in
    --home)
        data_home="$2/.local/share"
        shift 2
        ;;
    --seconds)
        seconds=$2
        shift 2
        ;;
    --help | -h)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
    esac
done

if [[ ! ${seconds} =~ ^[0-9]+$ ]]; then
    echo "--seconds takes a whole number of seconds, such as 45." >&2
    exit 2
fi
if [[ ${EUID} -eq 0 ]]; then
    echo "Run this as the throwaway account, not as root." >&2
    exit 1
fi

steam_dir="${data_home}/Steam"
for real in steamapps ubuntu12_32 ubuntu12_64 package steam.sh; do
    if [[ -e ${steam_dir}/${real} ]]; then
        echo "${steam_dir} has ${real}, so it looks like a real Steam. Use a throwaway HOME with --home, or a throwaway account." >&2
        exit 1
    fi
done

logs="${steam_dir}/logs"
mkdir -p "${logs}"

# A line as Steam writes it: its local time, then what it says.
steam_says() {
    printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$2" >>"${logs}/$1"
}

updater_says() {
    steam_says bootstrap_log.txt "$1"
}

# A stand-in whose name in /proc is exactly `steam`, as the updater's is.
stand_in_dir=$(mktemp -d)
stand_in_pid=""
stop_stand_in() {
    if [[ -n ${stand_in_pid} ]]; then
        kill "${stand_in_pid}" 2>/dev/null || true
        wait "${stand_in_pid}" 2>/dev/null || true
        stand_in_pid=""
    fi
    rm -rf "${stand_in_dir}"
}
trap stop_stand_in EXIT
cp /usr/bin/sleep "${stand_in_dir}/steam"
"${stand_in_dir}/steam" infinity &
stand_in_pid=$!
# A sleep that is one program of many decides what to be by its own name,
# and a copy called steam would stop at once.
sleep 0.2
if [[ $(cat "/proc/${stand_in_pid}/comm" 2>/dev/null) != steam ]]; then
    echo "The stand-in steam did not keep running: /usr/bin/sleep cannot run under another name here." >&2
    exit 1
fi

client="'${steam_dir}/ubuntu12_32/steam' '-srt-logger-opened'"
updater_says "Startup - updater built Sep  3 2026 01:31:48"
updater_says "Startup - Steam Client launched with: ${client}"
updater_says "Found pending update"
updater_says "Installing update..."
updater_says "Extracting package..."

echo "Steam is updating itself, by its logs in ${logs}, for ${seconds} s. Press Enter to end it sooner."
if [[ -t 0 ]]; then
    read -r -t "${seconds}" || true
else
    sleep "${seconds}"
fi

updater_says "Installing update..."
updater_says "Cleaning up..."
updater_says "Update complete, launching Steam..."
updater_says "Shutdown"
updater_says "Startup - updater built Sep  3 2026 01:31:48"
updater_says "Verifying installation..."
updater_says "Verification complete"
steam_says console_log.txt "System startup time: 16.79 seconds"
steam_says console_log.txt "ExecCommandLine: \"${client} '-silent'\""
stop_stand_in

echo "Steam has finished updating itself. Its games open again on the tiles within 5 s."
