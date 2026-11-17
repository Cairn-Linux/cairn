#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that the reports the image would send without anyone asking stay
# off (DESIGN §9.3, #36): Homebrew's usage reports, Flatpak's line naming
# the system, Tailscale's logs, firmware reports, the location service's
# Wi-Fi upload, and KDE's usage reports in an L3 or L4 child's login.
# Also that Family's entry stays out of that child's System Settings
# (DESIGN §3.2), which is not a report but is in the same file.
# Runs in its own step of the Containerfile after build.sh, so a base
# update that moves one of the pieces below stops the build until it is
# dealt with.

set -ouex pipefail

# set -x alone shows only the test that failed, so each failure also
# says in words what was found.
fail() {
    echo "$*" >&2
    exit 1
}

# The file has this whole line, spaces and all.
has_line() {
    local file="$1" line="$2"
    [[ -f "${file}" ]] || fail "${file} is missing"
    grep -qxF -- "${line}" "${file}" || fail "${file} does not say ${line}"
}

# None of the files after the pattern has a line that matches it.
no_line() {
    local pattern="$1" found
    shift
    found=$(grep -HEn -- "${pattern}" "$@" || true)
    [[ -z "${found}" ]] || fail "Found what must not be here: ${found}"
}

# The files in these folders, if there are any.
files_in() {
    local folder
    for folder in "$@"; do
        if [[ -d "${folder}" ]]; then
            find "${folder}" -type f
        fi
    done
}

# Long text is read from files here, not from variables: set -x would
# print a variable's whole text into the build log.
scratch=$(mktemp -d)
trap 'rm -r "${scratch}"' EXIT

# Homebrew. bin/brew reads /etc/homebrew/brew.env first, and again last
# when the file asks for it, so nobody's own file turns the reports back
# on. If Homebrew's copy stopped reading the file, its lines would do
# nothing.
brew_env=/etc/homebrew/brew.env
has_line "${brew_env}" HOMEBREW_NO_ANALYTICS=1
has_line "${brew_env}" HOMEBREW_SYSTEM_ENV_TAKES_PRIORITY=1
homebrew=/usr/share/homebrew.tar.zst
if [[ -e "${homebrew}" ]]; then
    brew="${scratch}/brew"
    tar --zstd -xOf "${homebrew}" home/linuxbrew/.linuxbrew/Homebrew/bin/brew \
        >"${brew}" || fail "${homebrew} has no Homebrew/bin/brew to read"
    reads=$(grep -cE "^[[:space:]]*export_homebrew_env_file \"${brew_env}\"\$" \
        "${brew}" || true)
    [[ "${reads}" == 2 ]] ||
        fail "Homebrew's bin/brew reads ${brew_env} ${reads} times, not first and last"
    grep -qF HOMEBREW_SYSTEM_ENV_TAKES_PRIORITY "${brew}" ||
        fail "Homebrew's bin/brew no longer knows HOMEBREW_SYSTEM_ENV_TAKES_PRIORITY"
fi

# Flatpak. The unit is there and enabled. With drop-ins included, it runs
# only while its done-file is missing, and it runs the setting, then
# makes that file, and nothing else. Without the file, or with the file
# at another path, it would set the setting back at every start and undo
# a grown-up's change: a lock, not a default. Cairn's preset enables it
# too, and it still sets a setting this flatpak knows. Each unit it is
# ordered against is still one the image has; a renamed one would leave
# it unordered.
unit=cairn-flatpak-os-report-off.service
unit_file=/usr/lib/systemd/system/${unit}
marker=/var/lib/cairn/flatpak-os-report-off.done
[[ -f "${unit_file}" ]] || fail "${unit_file} is missing"
link=/etc/systemd/system/multi-user.target.wants/${unit}
[[ -L "${link}" ]] || fail "${unit} is not enabled: ${link} is missing"
[[ "$(readlink -f "${link}")" == "${unit_file}" ]] ||
    fail "${link} leads to $(readlink -f "${link}"), not ${unit_file}"
state=$(systemctl is-enabled "${unit}" || true)
[[ "${state}" == enabled ]] || fail "systemctl says ${unit} is ${state}"
unit_merged="${scratch}/${unit}"
systemctl cat "${unit}" >"${unit_merged}" ||
    fail "systemctl cannot read ${unit}"
only_if="ConditionPathExists=!${marker}"
mapfile -t conditions < <(grep -E '^[[:space:]]*Condition' "${unit_merged}")
[[ "${conditions[*]}" == "${only_if}" ]] ||
    fail "${unit} must have only ${only_if}, but has: ${conditions[*]}"
set_it="ExecStart=/usr/bin/flatpak config --system --set report-os-info false"
mark_done="ExecStartPost=/usr/bin/touch ${marker}"
mapfile -t commands < <(grep -E '^[[:space:]]*Exec' "${unit_merged}")
[[ "${#commands[@]}" == 2 && "${commands[0]}" == "${set_it}" &&
    "${commands[1]}" == "${mark_done}" ]] ||
    fail "${unit} must run only ${set_it} and ${mark_done}, but runs: ${commands[*]}"
has_line /usr/lib/systemd/system-preset/40-cairn.preset "enable ${unit}"
grep -aqF report-os-info /usr/bin/flatpak ||
    fail "/usr/bin/flatpak no longer knows the setting report-os-info"
mapfile -t ordered < <(sed -nE 's/^(After|Before)=//p' "${unit_file}" |
    tr ' ' '\n')
[[ "${#ordered[@]}" -gt 0 ]] || fail "${unit} is ordered against nothing"
for other in "${ordered[@]}"; do
    [[ -e "/usr/lib/systemd/system/${other}" ]] ||
        fail "${unit} is ordered against ${other}, which the image no longer has"
done

# Tailscale. The drop-in is read, nothing else in the unit sets, unsets
# or passes the variable, and tailscaled still reads it. A line in the
# unit's EnvironmentFile would win over the drop-in, so the package's own
# file must not have one. With no Tailscale there is nothing to send.
if [[ -e /usr/lib/systemd/system/tailscaled.service ]]; then
    drop_in=/usr/lib/systemd/system/tailscaled.service.d/cairn-no-logs.conf
    has_line "${drop_in}" "Environment=TS_NO_LOGS_NO_SUPPORT=true"
    merged="${scratch}/tailscaled.service"
    systemctl cat tailscaled.service >"${merged}" ||
        fail "systemctl cannot read tailscaled.service"
    grep -qxF "# ${drop_in}" "${merged}" ||
        fail "systemd does not read ${drop_in} for tailscaled.service"
    settings=$(grep -E '^[^#;]*TS_NO_LOGS_NO_SUPPORT' "${merged}" || true)
    [[ "${settings}" == "Environment=TS_NO_LOGS_NO_SUPPORT=true" ]] ||
        fail "tailscaled.service has more than Cairn's line for TS_NO_LOGS_NO_SUPPORT: ${settings}"
    mapfile -t env_files < <(sed -nE 's/^EnvironmentFile=-?//p' "${merged}")
    for env_file in "${env_files[@]}"; do
        if [[ -e "${env_file}" ]]; then
            no_line TS_NO_LOGS_NO_SUPPORT "${env_file}"
        fi
    done
    grep -aqF TS_NO_LOGS_NO_SUPPORT /usr/sbin/tailscaled ||
        fail "/usr/sbin/tailscaled no longer reads TS_NO_LOGS_NO_SUPPORT"
fi

# Firmware: no report after an update and no security report, from any
# source of firmware. Already so in the base; this keeps it so.
lvfs=/etc/fwupd/remotes.d/lvfs.conf
has_line "${lvfs}" AutomaticReports=false
has_line "${lvfs}" AutomaticSecurityReports=false
mapfile -t remotes < <(files_in /etc/fwupd/remotes.d /usr/share/fwupd/remotes.d)
no_line '^[[:space:]]*Automatic(Security)?Reports[[:space:]]*=[[:space:]]*true' \
    "${remotes[@]}"

# The location service: no upload of the Wi-Fi networks nearby. Already
# so in the base. A file in conf.d is read after geoclue.conf and would
# win.
has_line /etc/geoclue/geoclue.conf submit-data=false
mapfile -t drop_ins < <(files_in /etc/geoclue/conf.d /usr/share/geoclue/conf.d)
if [[ -n "${drop_ins[*]}" ]]; then
    no_line '^[[:space:]]*submit-data[[:space:]]*=[[:space:]]*true' \
        "${drop_ins[@]}"
fi

# KDE, in an L3 or L4 child's login (ADR-0058): KDE's usage reports off
# for every KDE program, and System Settings' User Feedback page hidden.
# Qt takes the switch from the first folder in XDG_CONFIG_DIRS that has
# it, and cairn-session puts child-xdg after /etc/xdg and KDE's profile
# folder, so neither of those may set it. Each name is still the image's;
# a renamed one would hide nothing.
child_xdg=/usr/share/cairn/child-xdg
has_line "${child_xdg}/KDE/UserFeedback.conf" "[UserFeedback]"
has_line "${child_xdg}/KDE/UserFeedback.conf" Enabled=false
for folder in /etc/xdg /usr/share/kde-settings/kde-profile/default/xdg; do
    if [[ -e "${folder}/KDE/UserFeedback.conf" ]]; then
        no_line '^[[:space:]]*Enabled[[:space:]]*=' \
            "${folder}/KDE/UserFeedback.conf"
    fi
done
has_line "${child_xdg}/kdeglobals" "kcm_feedback[\$i]=false"
# Not a report: Family's entry is hidden from the child's System Settings
# too, since a Guardian's tools are invisible to children (DESIGN §3.2).
has_line "${child_xdg}/kdeglobals" "cairn-guardian-settings[\$i]=false"
[[ -e /usr/lib64/qt6/plugins/plasma/kcms/systemsettings/kcm_feedback.so ]] ||
    fail "System Settings has no kcm_feedback page any more"
[[ -e /usr/share/plasma/systemsettings/externalmodules/cairn-guardian-settings.desktop ]] ||
    fail "System Settings has no cairn-guardian-settings entry any more"
grep -aqF "Global kill switch is enabled" /usr/lib64/libKF6UserFeedbackCore.so.6 ||
    fail "KDE's feedback library no longer has its switch for every program"
