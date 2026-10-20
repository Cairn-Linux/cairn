#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Turn a stock Bazzite KDE install into a Phase 0 Cairn machine (ROADMAP
# P0-2, issue #1). Run as root from a checkout of this repository that has a
# release build in build/release:
#
#   sudo provision/cairn-provision.sh --guardian NAME --child NAME [--child NAME:2]
#
# Each --child is a child's account, at level 1 unless a :2 follows the name.
#
# Every step looks before it changes anything and says what it did, so a
# second run on the same machine reports zero changes. Throwaway by design:
# the Phase 1 image does all of this by construction (provision/README.md).
#
# The script traces every command (set -x) so a run can be read back, and
# for that reason no secret is ever traced: see set_guardian_password. The
# steps are functions and run from main, so the tests can load them with
# stand-ins for the system's commands (tests/test_provision.py).
set -ouex pipefail

# The physical path: on Bazzite /home is a link to /var/home, and
# install_file compares real paths against this one.
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
PREFIX=/usr/local
SHARE="$PREFIX/share/cairn"
FACTS=/var/lib/cairn/facts.txt
# Where a Guardian's Steam installs the games every child gets a copy of
# (ADR-0033).
SHARED_LIBRARY=/var/lib/cairn/steam-library
# The children's game tiles, made from the shared folder, and the picture
# each one shows, copied from the Guardian's Steam (ADR-0033, ADR-0035).
GAMES_FILE=/var/lib/cairn/games.json
GAME_PICTURES=/var/lib/cairn/game-pictures
LEVEL_GROUPS=(cairn-l1 cairn-l2 cairn-l3 cairn-l4 cairn-guardian)
# The brand's typefaces (DESIGN §6.2, brand/tokens.json) are in Fedora but
# not in the Bazzite image; without them every Cairn screen falls back to
# another font, losing the letterforms early readers need (I/l/1, O/0).
LAYERED_PACKAGES=(labwc scummvm sddm sddm-breeze sddm-wayland-plasma
    atkinson-hyperlegible-next-fonts atkinson-hyperlegible-mono-fonts)
FLATPAKS=(org.kde.gcompris org.tuxpaint.Tuxpaint)

guardian=""
# The children's accounts and their levels, in the same order.
children=()
levels=()
changes=0
reboot_needed=no

usage() {
    echo "usage: $0 --guardian NAME --child NAME[:LEVEL] [--child NAME[:LEVEL]]..." >&2
    echo "  LEVEL is 1 (the default) or 2, the two levels the kiosk serves." >&2
    echo "  CAIRN_GUARDIAN_PASSWORD sets the Guardian's password on creation." >&2
    exit 2
}

# One --child: a name, and a level after a colon if it is not 1. Levels 3
# and 4 are a desktop session no child has been given yet, so they wait.
add_child() {
    local name="${1%%:*}" level=1
    if [ "$1" != "$name" ]; then
        level="${1#*:}"
    fi
    case "$level" in
        1 | 2) ;;
        *) refuse "\"$level\" is not a level this script gives; use 1 or 2" ;;
    esac
    children+=("$name")
    levels+=("$level")
}

parse_arguments() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --guardian) guardian="$2"; shift 2 ;;
            --child) add_child "$2"; shift 2 ;;
            *) usage ;;
        esac
    done
    [ -n "$guardian" ] && [ "${#children[@]}" -gt 0 ] || usage
}

refuse() {
    echo "refused: $*" >&2
    exit 1
}

changed() {
    changes=$((changes + 1))
    echo "changed: $*"
}

# --- 1. Record what the stock system ships (feeds D6, issue #14) ---------------
record_facts() {
    mkdir -p "$(dirname "$FACTS")"
    {
        grep -E '^(PRETTY_NAME|IMAGE_ID)=' /etc/os-release
        echo "display-manager: $(readlink -f /etc/systemd/system/display-manager.service)"
        rpm -q sddm plasma-login-manager greetd || true
    } > "$FACTS.new"
    if ! cmp -s "$FACTS" "$FACTS.new" 2>/dev/null; then
        mv "$FACTS.new" "$FACTS"
    else
        rm "$FACTS.new"
    fi
    cat "$FACTS"
}

# --- 2. Level groups (ADR-0011) -----------------------------------------------
ensure_groups() {
    local group
    for group in "${LEVEL_GROUPS[@]}"; do
        if ! getent group "$group" > /dev/null; then
            groupadd --system "$group"
            changed "group $group"
        fi
    done
}

in_group() {
    id -nG "$1" | tr ' ' '\n' | grep -qx -- "$2"
}

# Exactly one level group per account. The wanted one is added before the
# others are dropped, so a run cut off in between leaves the account in two
# groups, which every check reads as the more restricted, and never in none,
# which the session would read as an account Cairn did not make (#100).
set_level_group() {
    local user="$1" wanted="$2" group
    if ! in_group "$user" "$wanted"; then
        gpasswd -a "$user" "$wanted"
        changed "$user joined $wanted"
    fi
    for group in "${LEVEL_GROUPS[@]}"; do
        if [ "$group" != "$wanted" ] && in_group "$user" "$group"; then
            gpasswd -d "$user" "$group"
            changed "$user left $group"
        fi
    done
}

# --- 3. Accounts ---------------------------------------------------------------
# The first uid useradd gives a person, from Fedora's login.defs. Anything
# lower is a system account.
UID_MIN=1000

# Checked before anything changes. The names go into useradd and into files
# under /etc, so they must be plain login names. The child must be a person's
# account that holds no power: an existing admin, system or Guardian account
# would otherwise be put in a child level and keep what it had. And the other
# way: an existing child named as the Guardian would be given wheel (#101).
check_accounts() {
    local name
    for name in "$guardian" "${children[@]}"; do
        [[ "$name" =~ ^[a-z_][a-z0-9_-]{0,31}$ ]] || refuse "\"$name\" is not a plain login name"
    done
    local child other
    for child in "${children[@]}"; do
        [ "$guardian" != "$child" ] || refuse "the Guardian and a child must be different accounts"
        local seen=0
        for other in "${children[@]}"; do
            [ "$other" != "$child" ] || seen=$((seen + 1))
        done
        [ "$seen" -eq 1 ] || refuse "$child is named twice; each child is one account"
        if id "$child" > /dev/null 2>&1; then
            [ "$(id -u "$child")" -ge "$UID_MIN" ] || refuse "$child is a system account"
            local group
            for group in wheel cairn-guardian; do
                if in_group "$child" "$group"; then
                    refuse "$child is in $group; a child account must not administer the machine"
                fi
            done
        fi
    done
    if id "$guardian" > /dev/null 2>&1; then
        [ "$(id -u "$guardian")" -ge "$UID_MIN" ] || refuse "$guardian is a system account"
        local level
        for level in cairn-l1 cairn-l2 cairn-l3 cairn-l4; do
            if in_group "$guardian" "$level"; then
                refuse "$guardian is in $level; a child account cannot be made a Guardian here"
            fi
        done
    fi
}

# The password must never reach the trace, a log or a command line (#103).
# At a terminal, passwd asks for it: it does not echo, asks twice, and this
# script never sees it. CAIRN_GUARDIAN_PASSWORD is for a run from a root
# shell; tracing is off for this whole step, including the test that the
# variable is set, which would show its value too. printf is a shell builtin,
# so the password is never an argument another process could read; chpasswd
# reads it on stdin. The variable is dropped afterwards.
set_guardian_password() {
    { set +x; } 2> /dev/null
    if [ -n "${CAIRN_GUARDIAN_PASSWORD:-}" ]; then
        printf '%s:%s\n' "$guardian" "$CAIRN_GUARDIAN_PASSWORD" | chpasswd
        changed "password set for $guardian"
    elif [ -t 0 ] && passwd "$guardian"; then
        changed "password set for $guardian"
    else
        echo "note: $guardian has no password yet; set one with: passwd $guardian"
    fi
    unset CAIRN_GUARDIAN_PASSWORD
    set -x
}

ensure_guardian() {
    if ! id "$guardian" > /dev/null 2>&1; then
        useradd --create-home --groups wheel "$guardian"
        changed "guardian account $guardian"
        set_guardian_password
    fi
    set_level_group "$guardian" cairn-guardian
}

ensure_children() {
    local i child
    for i in "${!children[@]}"; do
        child="${children[$i]}"
        if ! id "$child" > /dev/null 2>&1; then
            # No password: the greeter lets an L1 or L2 child in without one
            # (P0-3), and a locked password keeps su and ssh out (issue #35).
            useradd --create-home "$child"
            changed "child account $child"
        fi
        if [ "$(passwd --status "$child" | awk '{print $2}')" != "L" ]; then
            passwd --lock "$child"
            changed "$child password locked"
        fi
        set_level_group "$child" "cairn-l${levels[$i]}"
    done
}

# --- 4. Packages: the kiosk compositor and ScummVM live in the OS -------------
# A package layered by an earlier run is not in the booted deployment until
# the reboot, so rpm-ostree's own list of layered packages counts too.
# Installed, or asked for in the newest deployment and waiting for a reboot.
# The JSON, because the text form wraps a long LayeredPackages list over
# several lines.
package_present() {
    rpm -q "$1" > /dev/null 2>&1 && return
    rpm-ostree status --json | python3 -c '
import json, sys
newest = json.load(sys.stdin)["deployments"][0]
sys.exit(0 if sys.argv[1] in newest.get("requested-packages", []) else 1)
' "$1"
}

# sddm requires desktop-backgrounds-compat, whose two wallpaper paths exist
# in the Bazzite image as symlinks no package owns, so a plain layering
# fails on the file clash. rpm-ostree allows the replacement only for a
# local RPM in a transaction of its own, so that package goes first.
layer_backgrounds_compat() {
    package_present desktop-backgrounds-compat && return
    local dir
    dir="$(mktemp -d)"
    dnf5 download --quiet --destdir "$dir" desktop-backgrounds-compat
    # Installed as root with files forced over the image's, so it must be
    # Fedora's own package: its signature is checked against the keys the
    # system already trusts.
    rpm -K "$dir"/desktop-backgrounds-compat-*.rpm
    rpm-ostree install --idempotent --force-replacefiles "$dir"/desktop-backgrounds-compat-*.rpm
    rm -r "$dir"
    changed "layered desktop-backgrounds-compat over the image's wallpaper symlinks"
    reboot_needed=yes
}

layer_packages() {
    local missing=()
    local package
    for package in "${LAYERED_PACKAGES[@]}"; do
        package_present "$package" || missing+=("$package")
    done
    if [ ${#missing[@]} -gt 0 ]; then
        layer_backgrounds_compat
        rpm-ostree install --idempotent "${missing[@]}"
        changed "layered ${missing[*]} (reboot to use them)"
        reboot_needed=yes
    fi
}

# --- 5. Flatpaks: everything a Guardian could add later arrives this way --------
install_flatpaks() {
    local missing=()
    local app
    for app in "${FLATPAKS[@]}"; do
        flatpak info --system "$app" > /dev/null 2>&1 || missing+=("$app")
    done
    if [ ${#missing[@]} -gt 0 ]; then
        flatpak install --system --noninteractive --assumeyes flathub "${missing[@]}"
        changed "flatpaks ${missing[*]}"
    fi
}

# --- 6. Files: kiosk configuration, launcher, manifest --------------------------
# Written beside the target and renamed into place, so a daemon that watches
# the directory (polkitd does) sees a whole file rather than one mid-write.
# Installed as root, often world-readable, so a source must be a regular
# file, and one inside the checkout must stay inside it: a link in the tree
# cannot carry some other file (a key, /etc/shadow) out into /usr/local.
# The theme's mark is a link to brand/, which is inside, so it passes. The
# only sources outside the checkout are files this script writes itself.
install_file() {
    local mode="$1" source="$2" target="$3" real
    real="$(realpath -e "$source")" || refuse "$source does not exist"
    [ -f "$real" ] || refuse "$source is not a regular file"
    case "$real" in
        "$REPO"/*) ;;
        *) [ "$source" = "${source#"$REPO"/}" ] || refuse "$source leads outside the checkout" ;;
    esac
    if ! cmp -s "$source" "$target"; then
        install -D --mode="$mode" "$source" "$target.cairn-new"
        mv "$target.cairn-new" "$target"
        changed "$target"
    fi
}

install_session_files() {
    install_file 644 "$REPO/session/labwc/rc.xml" "$SHARE/labwc/rc.xml"
    install_file 644 "$REPO/session/labwc/environment" "$SHARE/labwc/environment"
    # The one session entry and its dispatcher (ADR-0012).
    install_file 755 "$REPO/session/bin/cairn-session" "$PREFIX/bin/cairn-session"
    # The grown-up's way out of a stuck program (ADR-0018, issue #41).
    install_file 755 "$REPO/session/bin/cairn-give-up" "$PREFIX/bin/cairn-give-up"
    # The child's own Log out in the launcher (ADR-0021, issue #44).
    install_file 755 "$REPO/session/bin/cairn-log-out" "$PREFIX/bin/cairn-log-out"
    install_file 644 "$REPO/session/sessions/cairn.desktop" "$SHARE/sessions/cairn.desktop"
    # What a child's session may ask the system to do (issue #35). polkitd
    # watches the directory, so the rule applies without a restart.
    install_file 644 "$REPO/session/polkit/rules.d/10-cairn-levels.rules" /etc/polkit-1/rules.d/10-cairn-levels.rules
    # The power button: a tap is ignored, a hold powers off (ADR-0018).
    # logind reads its configuration at start, so this waits for a boot.
    local before=$changes
    install_file 644 "$REPO/session/logind.conf.d/10-cairn-power.conf" /etc/systemd/logind.conf.d/10-cairn-power.conf
    if [ "$changes" != "$before" ]; then
        reboot_needed=yes
    fi
}

# --- 7. The display manager: SDDM, swapped in explicitly (ADR-0016) ---------
install_display_manager() {
    install_file 644 "$REPO/session/sddm/sddm.conf.d/10-cairn.conf" /etc/sddm.conf.d/10-cairn.conf
    install_file 644 "$REPO/session/sddm/sddm.conf.d/zz-cairn-no-autologin.conf" /etc/sddm.conf.d/zz-cairn-no-autologin.conf
    install_file 644 "$REPO/session/sddm/pam.d/sddm" /etc/pam.d/sddm
    # Nobody is hidden from the login screen since ADR-0020; an earlier run
    # wrote the Guardian list here.
    if [ -e /etc/sddm.conf.d/20-cairn-guardians.conf ]; then
        rm /etc/sddm.conf.d/20-cairn-guardians.conf
        changed "/etc/sddm.conf.d/20-cairn-guardians.conf removed"
    fi
    # The packages are layered above; until the reboot sddm.service is not
    # there to enable, so this step waits for the second run.
    if [ -f /usr/lib/systemd/system/sddm.service ]; then
        if [ "$(systemctl is-enabled plasmalogin.service 2>/dev/null)" != "disabled" ]; then
            systemctl disable plasmalogin.service
            changed "plasmalogin.service disabled"
        fi
        if [ "$(systemctl is-enabled sddm.service 2>/dev/null)" != "enabled" ]; then
            systemctl enable sddm.service
            changed "sddm.service enabled (takes effect at the next boot)"
            reboot_needed=yes
        fi
    else
        echo "note: sddm is not in the booted deployment yet; rerun after the reboot to enable it"
    fi
}

install_launcher() {
    local build="$REPO/build/release"
    install_file 755 "$build/launcher/cairn-launcher" "$PREFIX/libexec/cairn/cairn-launcher"
    install_file 755 "$build/brand/qml/Cairn/Brand/libCairnBrand.so" "$PREFIX/lib64/cairn/libCairnBrand.so"
    install_file 644 "$REPO/provision/manifest.json" "$SHARE/manifest.json"
    # The binary finds its one shared library through the wrapper, which the
    # session dispatcher will call. Phase 1 packaging sets an rpath instead.
    local wrapper
    wrapper="$(mktemp)"
    cat > "$wrapper" <<'WRAPPER'
#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Installed by provision/cairn-provision.sh; not the Phase 1 packaging.
export LD_LIBRARY_PATH=/usr/local/lib64/cairn
launcher=(/usr/local/libexec/cairn/cairn-launcher --manifest /usr/local/share/cairn/manifest.json
    --games /var/lib/cairn/games.json --log-out /usr/local/bin/cairn-log-out --scope-apps --start-steam "$@")
# The launcher's notes for a grown-up go to the journal under their own name
# (#122): journalctl -t cairn-launcher. No journal must never mean no tiles.
if systemd-cat --identifier=cairn-launcher true > /dev/null 2>&1; then
    exec systemd-cat --identifier=cairn-launcher "${launcher[@]}"
fi
exec "${launcher[@]}"
WRAPPER
    install_file 755 "$wrapper" "$PREFIX/bin/cairn-launcher"
    rm "$wrapper"
}

# --- 8. The login screen: the Cairn theme and its QML modules (ADR-0020) ----
install_greeter() {
    local build="$REPO/build/release" qml="$PREFIX/lib64/cairn/qml" theme="$SHARE/sddm/themes/cairn"
    local file
    for file in Main.qml FamilyTile.qml PasswordRow.qml TextButton.qml metadata.desktop theme.conf mark-on-ink.svg; do
        install_file 644 "$REPO/greeter/theme/$file" "$theme/$file"
    done
    # The tokens as plain QML, so the greeter needs no brand library.
    install_file 644 "$REPO/brand/qml/Cairn/Brand/qmldir" "$qml/Cairn/Brand/qmldir"
    install_file 644 "$REPO/brand/qml/Cairn/Brand/Tokens.qml" "$qml/Cairn/Brand/Tokens.qml"
    # The plugin finds the library beside it through its rpath.
    install_file 644 "$build/qml/Cairn/Greeter/qmldir" "$qml/Cairn/Greeter/qmldir"
    install_file 644 "$build/qml/Cairn/Greeter/cairngreeter.qmltypes" "$qml/Cairn/Greeter/cairngreeter.qmltypes"
    install_file 755 "$build/qml/Cairn/Greeter/libcairngreeterplugin.so" "$qml/Cairn/Greeter/libcairngreeterplugin.so"
    install_file 755 "$build/greeter/libcairngreeter.so" "$qml/Cairn/Greeter/libcairngreeter.so"
}

# --- 9. The Guardian's password: a lockout that survives a reboot (ADR-0020) -
protect_guardians() {
    install_file 644 "$REPO/session/security/faillock.conf" /etc/security/faillock.conf
    # Only the local rules, read to the end: grep -q would stop at the first
    # match, semanage would die of SIGPIPE, and pipefail would call it a miss.
    if ! semanage fcontext -l -C | grep -F '/var/lib/faillock(/.*)?' > /dev/null; then
        semanage fcontext -a -t faillog_t '/var/lib/faillock(/.*)?'
        changed "SELinux label for /var/lib/faillock"
    fi
    if [ ! -d /var/lib/faillock ]; then
        install -d --mode=755 /var/lib/faillock
        changed "/var/lib/faillock"
    fi
    restorecon -R /var/lib/faillock
    if ! authselect current | grep -qx -- '- with-faillock'; then
        authselect enable-feature with-faillock
        changed "authselect with-faillock"
    fi
}

# --- 10. A grown-up's way into a child's Steam (ADR-0032) --------------------
install_steam_visit() {
    local cage="$PREFIX/libexec/cairn/cage-visit"
    install_file 755 "$REPO/session/bin/cairn-steam-visit" "$PREFIX/libexec/cairn/cairn-steam-visit"
    # A copy of the image's cage that the child may run but not read: Linux
    # then keeps the child's other programs from tracing it or copying its
    # connection to the Guardian's desktop. Copied again whenever the
    # image's cage changes. The mode is checked on its own, because
    # install_file compares only what is in the file.
    install_file 711 /usr/bin/cage "$cage"
    if [ "$(stat --format=%a "$cage")" != 711 ]; then
        chmod 711 "$cage"
        changed "$cage made run-only"
    fi
    install_file 644 "$REPO/session/pam.d/cairn-steam-visit" /etc/pam.d/cairn-steam-visit
    install_file 644 "$REPO/session/pam.d/login" /etc/pam.d/login
    # pkexec finds the action by the helper's real path, and on Bazzite
    # /usr/local is a link to /var/usrlocal, so the action names the path
    # the link leads to. polkitd does not see an actions directory made
    # after it started, so it is restarted when the action is new or changed.
    local action before=$changes
    action="$(mktemp)"
    sed "s|>/usr/local/libexec/cairn/cairn-steam-visit<|>$(realpath -e "$PREFIX/libexec/cairn/cairn-steam-visit")<|" \
        "$REPO/session/polkit/actions/com.cairnlinux.steam-visit.policy" > "$action"
    install_file 644 "$action" /etc/polkit-1/actions/com.cairnlinux.steam-visit.policy
    rm "$action"
    if [ "$changes" != "$before" ]; then
        systemctl restart polkit.service
    fi
    # The login screen and the consoles read the list of children on a
    # visit, and SELinux lets them only with this label. PAM lets everyone
    # in when it cannot read the list, so the helper checks the label too.
    if ! semanage fcontext -l -C | grep -F '/run/cairn(/.*)?' > /dev/null; then
        semanage fcontext -a -t etc_t '/run/cairn(/.*)?'
        changed "SELinux label for /run/cairn"
    fi
}

# --- 11. "Steam for <child>" in a Guardian's menu (ADR-0032) -----------------
# Every child account on the machine, from the level groups, whichever run
# made it. An administrator is never one, whatever groups it is in.
child_accounts() {
    local level member
    for level in cairn-l1 cairn-l2 cairn-l3 cairn-l4; do
        for member in $(getent group "$level" | cut -d: -f4 | tr ',' ' '); do
            in_group "$member" wheel || in_group "$member" cairn-guardian || echo "$member"
        done
    done | sort -u
}

# The name the family knows an account by, as the login screen shows it.
known_as() {
    local name
    name="$(getent passwd "$1" | cut -d: -f5 | cut -d, -f1)"
    echo "${name:-$1}"
}

steam_entry() {
    local name
    name="$(known_as "$1")"
    cat <<ENTRY
[Desktop Entry]
Type=Application
Name=Steam for $name
Comment=Open $name's own Steam, to sign in or change its settings
Exec=$PREFIX/bin/cairn-steam-for $1
TryExec=$PREFIX/bin/cairn-steam-for
Icon=steam
Categories=Game;
ENTRY
}

# Only a Guardian may run it. Plasma's menu leaves out an entry whose
# TryExec program the account cannot run, so no one else sees the entries,
# and a child who finds the program cannot start it.
only_for_guardians() {
    if [ "$(stat --format=%G:%a "$1")" != cairn-guardian:750 ]; then
        chgrp cairn-guardian "$1"
        chmod 750 "$1"
        changed "$1 only for Guardians"
    fi
}

install_steam_for() {
    local build="$REPO/build/release"
    install_file 750 "$build/guardian/cairn-steam-for" "$PREFIX/libexec/cairn/cairn-steam-for"
    only_for_guardians "$PREFIX/libexec/cairn/cairn-steam-for"
    local wrapper
    wrapper="$(mktemp)"
    cat > "$wrapper" <<'WINDOW'
#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Installed by provision/cairn-provision.sh; not the Phase 1 packaging.
export LD_LIBRARY_PATH=/usr/local/lib64/cairn
window=(/usr/local/libexec/cairn/cairn-steam-for "$@")
# The window's notes and the root helper's trace go to the journal under
# their own name: journalctl -t cairn-steam-for. systemd-cat runs the window
# in its own place, so the window stays the parent pkexec needs.
if systemd-cat --identifier=cairn-steam-for true > /dev/null 2>&1; then
    exec systemd-cat --identifier=cairn-steam-for "${window[@]}"
fi
exec "${window[@]}"
WINDOW
    install_file 750 "$wrapper" "$PREFIX/bin/cairn-steam-for"
    rm "$wrapper"
    only_for_guardians "$PREFIX/bin/cairn-steam-for"

    local apps="$PREFIX/share/applications" kids child entry
    kids="$(child_accounts)"
    for child in $kids; do
        entry="$(mktemp)"
        steam_entry "$child" > "$entry"
        install_file 644 "$entry" "$apps/cairn-steam-for-$child.desktop"
        rm "$entry"
    done
    # An account that is no longer a child loses its entry.
    for entry in "$apps"/cairn-steam-for-*.desktop; do
        [ -e "$entry" ] || continue
        child="${entry##*/cairn-steam-for-}"
        child="${child%.desktop}"
        if ! grep -qx -- "$child" <<< "$kids"; then
            rm -- "$entry"
            changed "$entry removed"
        fi
    done
}

# --- 12. The games every child gets a copy of (ADR-0033) ----------------------
# Only Guardians may change the shared folder; every account may read it.
# The default ACL gives the group what Steam's own modes ask for, whatever a
# Guardian's umask, and setgid keeps the group on what Steam makes inside.
# Only these two folders: the games inside are Steam's, and never walked.
SHARED_ACL="user::rwx
group::rwx
group:cairn-guardian:rwx
mask::rwx
other::r-x
default:user::rwx
default:group::rwx
default:group:cairn-guardian:rwx
default:mask::rwx
default:other::r-x"

make_shared_library() {
    local folder
    for folder in "$SHARED_LIBRARY" "$SHARED_LIBRARY/steamapps"; do
        # chown, chmod and setfacl follow links, and a Guardian can write the
        # top folder.
        [ ! -L "$folder" ] || refuse "$folder is a link; move it aside and run this again"
        if [ ! -d "$folder" ]; then
            mkdir --mode=2775 "$folder"
            changed "$folder"
        fi
        if [ "$(stat --format=%U:%G:%a -- "$folder")" != root:cairn-guardian:2775 ]; then
            chown root:cairn-guardian "$folder"
            chmod 2775 "$folder"
            changed "$folder owned by root and cairn-guardian, mode 2775"
        fi
        if [ "$(getfacl --omit-header --absolute-names -- "$folder")" != "$SHARED_ACL" ]; then
            setfacl --set "u::rwx,g::rwx,g:cairn-guardian:rwx,m::rwx,o::r-x,d:u::rwx,d:g::rwx,d:g:cairn-guardian:rwx,d:m::rwx,d:o::r-x" -- "$folder"
            changed "$folder ACL for cairn-guardian"
        fi
    done
}

# The copy step, the picture step and kidscan, whose rules both share,
# where every account can read them and only root can change them. The copy
# runs as each child, and the checkout sits in the Guardian's own home,
# which children cannot open.
install_copy_games() {
    install_file 644 "$REPO/provision/cairn-copy-games.py" "$PREFIX/libexec/cairn/cairn-copy-games"
    install_file 644 "$REPO/provision/cairn-game-pictures.py" "$PREFIX/libexec/cairn/cairn-game-pictures"
    install_file 644 "$REPO/tools/kidscan/kidscan.py" "$PREFIX/libexec/cairn/kidscan.py"
}

# A game's name as the Guardian's Steam wrote it in the shared folder, with
# anything that is not printable taken out, or its number. This runs as root
# and every Guardian can write there, so the manifest is read by kidscan's
# own rules: not through a link, never waiting on a FIFO, and at most 1 MiB.
game_name() {
    local name
    name="$(python3 -I -B -c '
import sys
sys.path.insert(0, sys.argv[1])
name = ""
try:
    import kidscan
    text = kidscan.read_manifest(sys.argv[2])
    state = None if text is None else kidscan.ci_get(kidscan.parse_vdf(text), "AppState")
    name = kidscan.ci_get(state, "name", "") if isinstance(state, dict) else ""
except Exception:
    name = ""
print("".join(c for c in name if c.isprintable()) if isinstance(name, str) else "")
' "$PREFIX/libexec/cairn" "$SHARED_LIBRARY/steamapps/appmanifest_$1.acf" 2> /dev/null)" || name=""
    echo "${name:-game $1}"
}

# Why the copy step left a game out, from its reason word.
not_copied_because() {
    local child="$2"
    case "$1" in
        unreadable) echo "Steam's file for it cannot be read" ;;
        appid) echo "Steam's file for it names another game" ;;
        installdir) echo "Steam's file for it names its folder in a way Cairn does not copy" ;;
        beta-branch) echo "it is on a beta branch, which $child's Steam account may not have" ;;
        not-installed) echo "it is not fully installed, or an update is waiting; let your own Steam finish it" ;;
        downloading) echo "Steam is still downloading it" ;;
        no-folder) echo "its folder is missing from the shared folder" ;;
        folder-without-manifest) echo "$child's Steam has a folder for it already, which may hold $child's saves; docs/TRY.md says how to set it aside" ;;
        changed) echo "Steam changed it during the copy; it is copied at the next run" ;;
        copy-failed) echo "the copy did not work; it is tried again at the next run" ;;
        unreadable-files) echo "some of its files can be read only by your own account, so they cannot be copied; docs/TRY.md says what to do" ;;
        *) echo "Cairn does not copy it" ;;
    esac
}

# Why the helper refused to copy a child's games, from its reason word.
not_copying_because() {
    local child="$2"
    case "$1" in
        logged-in) echo "$child is logged in; their games are copied at the next run with $child logged out" ;;
        busy) echo "a child's Steam is open from a grown-up's login; $child's games are copied at the next run" ;;
        still-running) echo "a program of $child's is still running; their games are copied at the next run" ;;
        not-set-up) echo "no games are copied for $child; the list that keeps a child from logging in meanwhile is not labelled yet" ;;
        *) echo "no games are copied for $child; the helper refused: $1" ;;
    esac
}

# One line from the helper's copy mode, as a sentence of this script's own.
# The words are checked before any is used, and a line that is not one of
# them is never printed.
copy_sentence() {
    local child="$1" line="$2" appid reason
    if [[ "$line" =~ ^copied\ ([0-9]{1,10})$ ]]; then
        changed "$child's own copy of $(game_name "${BASH_REMATCH[1]}")"
    elif [[ "$line" =~ ^have\ [0-9]{1,10}$ ]]; then
        # The child's copy holds their saves, so it stays as it is.
        :
    elif [[ "$line" =~ ^skipped\ ([0-9]{1,10})\ ([a-z-]{1,32})$ ]]; then
        appid="${BASH_REMATCH[1]}"
        reason="${BASH_REMATCH[2]}"
        echo "note: $(game_name "$appid") is not copied for $child; $(not_copied_because "$reason" "$child")"
    elif [[ "$line" =~ ^refused\ ([a-z-]{1,32})$ ]]; then
        echo "note: $(not_copying_because "${BASH_REMATCH[1]}" "$child")"
    else
        case "$line" in
            waiting | finished) ;;
            unnumbered) echo "note: a file in the shared folder looks like a game but has no game number; it is not copied for $child" ;;
            no-library) echo "note: $child's Steam has not run yet; their games are copied at the next run after a visit signs it in" ;;
            no-shared) echo "note: there is no shared game folder, so no games are copied for $child" ;;
            lists-shared) echo "note: $child's Steam lists the shared game folder as its own, so no games are copied for $child; remove it in Steam's Settings > Storage during a visit" ;;
            unreadable-config) echo "note: no games are copied for $child; their Steam's list of game folders is not a plain file, so Cairn does not read it" ;;
            no-reflink) echo "note: no games are copied for $child; this computer's disk cannot make a copy that shares the original's space, and full copies would fill it" ;;
            failed) echo "note: copying $child's games stopped early; it is tried again at the next run, and journalctl -t cairn-steam-visit has a record" ;;
            *) echo "note: copying $child's games printed a line this script does not know; it is left out here" ;;
        esac
    fi
}

# --- 13. The children's game tiles, with pictures (ADR-0033, ADR-0035) -------
# The Guardian's home, where their own Steam keeps each game's library art.
# It comes from the account database, never from $HOME, which is root's
# here. Nothing when there is no such account; the picture step then finds
# no pictures.
guardian_home() {
    local home
    home="$(getent passwd "$guardian" | cut -d: -f6)" || home=""
    echo "$home"
}

# Why the picture step left a picture out, from its reason word.
picture_refused_because() {
    case "$1" in
        not-a-file) echo "is not a plain file" ;;
        too-big) echo "is larger than 1 MB" ;;
        not-a-jpeg) echo "is not a JPEG picture" ;;
        unreadable) echo "could not be read" ;;
    esac
}

# One line from the picture step, as a sentence of this script's own. The
# words are checked before any is used, and a line that is not one of them
# is never printed.
picture_sentence() {
    local line="$1" appid reason shows
    if [[ "$line" =~ ^copied\ ([0-9]{1,10})$ ]]; then
        changed "the picture for $(game_name "${BASH_REMATCH[1]}")"
    elif [[ "$line" =~ ^removed\ ([0-9]{1,10})$ ]]; then
        changed "the picture for $(game_name "${BASH_REMATCH[1]}"), which has no tile now, removed"
    elif [[ "$line" =~ ^missing\ ([0-9]{1,10})$ ]]; then
        echo "note: $(game_name "${BASH_REMATCH[1]}") has no picture yet, so its tile shows its name. Your own Steam keeps a game's picture once its library has shown the game: open Steam in your own login, then run this again."
    elif [[ "$line" =~ ^refused\ ([0-9]{1,10})\ (not-a-file|too-big|not-a-jpeg|unreadable)$ ]]; then
        appid="${BASH_REMATCH[1]}"
        reason="${BASH_REMATCH[2]}"
        # The picture step keeps a picture from an earlier run, and kidscan
        # names it when it is a plain file.
        if [ ! -L "$GAME_PICTURES/$appid.jpg" ] && [ -f "$GAME_PICTURES/$appid.jpg" ]; then
            shows="keeps the picture from an earlier run"
        else
            shows="shows its name"
        fi
        echo "note: the picture your Steam keeps for $(game_name "$appid") was left out, because it $(picture_refused_because "$reason"), so its tile $shows."
    fi
}

# A picture for each shared game, copied from the Guardian's own Steam by
# root into a folder only root can change, then the games file, from
# kidscan over the shared folder only, as root with an empty HOME, so that
# no home's Steam is read for it. This runs before the copies, which stop
# early for a child who is logged in. The trace is off while the picture
# step's lines are read, because it would print each line before it is
# checked.
make_game_tiles() {
    local home line ended=no games
    # make_shared_library refuses a link here, so one now was put here while
    # this ran. The picture step and kidscan refuse one that comes later
    # still.
    if [ -L "$SHARED_LIBRARY/steamapps" ]; then
        echo "note: $SHARED_LIBRARY/steamapps is a link, which Cairn does not follow, so the game tiles were not made again; the children keep the ones from the last run."
        return 0
    fi
    # With no shared folder the tiles from the last run stay.
    [ -d "$SHARED_LIBRARY/steamapps" ] || return 0
    # The picture step writes in it as root.
    [ ! -L "$GAME_PICTURES" ] || refuse "$GAME_PICTURES is a link; move it aside and run this again"
    [ -d "$GAME_PICTURES" ] || [ ! -e "$GAME_PICTURES" ] || refuse "$GAME_PICTURES is not a folder; move it aside and run this again"
    if [ ! -d "$GAME_PICTURES" ]; then
        mkdir --mode=755 "$GAME_PICTURES"
        changed "$GAME_PICTURES"
    fi
    # Only root may change a child's tiles; every child reads them.
    if [ "$(stat --format=%U:%G:%a -- "$GAME_PICTURES")" != root:root:755 ]; then
        chown root:root "$GAME_PICTURES"
        chmod 755 "$GAME_PICTURES"
        changed "$GAME_PICTURES owned by root, mode 755"
    fi
    home="$(guardian_home)"
    { set +x; } 2> /dev/null
    while IFS= read -r line; do
        picture_sentence "$line"
        if [ "$line" = finished ]; then
            ended=yes
        fi
    done < <(python3 -I -B "$PREFIX/libexec/cairn/cairn-game-pictures" --home "$home" \
        --shared "$SHARED_LIBRARY" --pictures "$GAME_PICTURES" < /dev/null 2> /dev/null || true)
    if [ "$ended" = no ]; then
        echo "note: the game pictures were not all made; a tile without one shows the game's name."
    fi
    set -x
    # install_file reports a change only when the bytes differ.
    games="$(mktemp)"
    if env HOME= python3 -I -B "$PREFIX/libexec/cairn/kidscan.py" --no-scummvm \
        --library "$SHARED_LIBRARY" --pictures "$GAME_PICTURES" > "$games" 2> /dev/null; then
        install_file 644 "$games" "$GAMES_FILE"
    else
        echo "note: the game tiles could not be made again; the children keep the ones from the last run."
    fi
    rm "$games"
}

# --- 14. Each child's own copy of the games (ADR-0033) -------------------------
# One child at a time, through the visit helper, which keeps the child from
# logging in while it runs and runs the copy as the child. A child who cannot
# be copied for now gets a note, and the others still get their copies. The
# trace is off, because it would print each line before it is checked. The
# helper's own trace goes nowhere; it keeps its record in the journal.
copy_games() {
    local child line ended
    # With no game in the shared folder no child is held from logging in, and
    # there is nothing to say.
    compgen -G "$SHARED_LIBRARY/steamapps/appmanifest_*.acf" > /dev/null || return 0
    { set +x; } 2> /dev/null
    for child in $(child_accounts); do
        ended=no
        while IFS= read -r line; do
            copy_sentence "$child" "$line"
            if [[ "$line" =~ ^(finished|failed|refused\ [a-z-]{1,32})$ ]]; then
                ended=yes
            fi
        done < <("$PREFIX/libexec/cairn/cairn-steam-visit" --copy-games "$child" < /dev/null 2> /dev/null || true)
        if [ "$ended" = no ]; then
            echo "note: copying $child's games stopped without saying why; it is tried again at the next run, and journalctl -t cairn-steam-visit may have a record"
        fi
    done
    set -x
}

relabel() {
    if command -v restorecon > /dev/null; then
        restorecon -R "$PREFIX/bin/cairn-launcher" "$PREFIX/bin/cairn-session" \
            "$PREFIX/bin/cairn-give-up" "$PREFIX/bin/cairn-log-out" "$PREFIX/bin/cairn-steam-for" \
            "$PREFIX/share/applications" \
            "$PREFIX/libexec/cairn" "$PREFIX/lib64/cairn" "$SHARE" \
            /etc/sddm.conf.d /etc/pam.d/sddm /etc/polkit-1/rules.d /etc/systemd/logind.conf.d \
            /etc/security/faillock.conf /etc/pam.d/cairn-steam-visit /etc/pam.d/login \
            /etc/polkit-1/actions
    fi
}

main() {
    parse_arguments "$@"
    [ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }
    check_accounts

    record_facts
    ensure_groups
    ensure_guardian
    ensure_children
    layer_packages
    install_flatpaks
    install_session_files
    install_launcher
    install_display_manager
    install_greeter
    protect_guardians
    install_steam_visit
    install_steam_for
    make_shared_library
    install_copy_games
    relabel
    make_game_tiles
    copy_games

    set +x
    echo "done: $changes change(s)"
    if [ "$reboot_needed" = yes ]; then
        echo "reboot: layered packages or the display manager change take effect at the next boot"
    fi
}

# Run when executed; only load the steps when a test sources the file.
if [ "${BASH_SOURCE[0]}" = "$0" ]; then
    main "$@"
fi
