#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Turn a stock Bazzite KDE install into a Phase 0 Cairn machine (ROADMAP
# P0-2, issue #1). Run as root from a checkout of this repository that has a
# release build in build/release:
#
#   sudo provision/cairn-provision.sh --guardian NAME --child NAME
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
LEVEL_GROUPS=(cairn-l1 cairn-l2 cairn-l3 cairn-l4 cairn-guardian)
# The brand's typefaces (DESIGN §6.2, brand/tokens.json) are in Fedora but
# not in the Bazzite image; without them every Cairn screen falls back to
# another font, losing the letterforms early readers need (I/l/1, O/0).
LAYERED_PACKAGES=(labwc scummvm sddm sddm-breeze sddm-wayland-plasma
    atkinson-hyperlegible-next-fonts atkinson-hyperlegible-mono-fonts)
FLATPAKS=(org.kde.gcompris org.tuxpaint.Tuxpaint)

guardian=""
child=""
changes=0
reboot_needed=no

usage() {
    echo "usage: $0 --guardian NAME --child NAME" >&2
    echo "  CAIRN_GUARDIAN_PASSWORD sets the Guardian's password on creation." >&2
    exit 2
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
    for name in "$guardian" "$child"; do
        [[ "$name" =~ ^[a-z_][a-z0-9_-]{0,31}$ ]] || refuse "\"$name\" is not a plain login name"
    done
    [ "$guardian" != "$child" ] || refuse "the Guardian and the child must be different accounts"
    if id "$child" > /dev/null 2>&1; then
        [ "$(id -u "$child")" -ge "$UID_MIN" ] || refuse "$child is a system account"
        local group
        for group in wheel cairn-guardian; do
            if in_group "$child" "$group"; then
                refuse "$child is in $group; a child account must not administer the machine"
            fi
        done
    fi
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

ensure_child() {
    if ! id "$child" > /dev/null 2>&1; then
        # No password: the greeter lets an L1 child in without one (P0-3),
        # and a locked password keeps su and ssh out (issue #35).
        useradd --create-home "$child"
        changed "child account $child"
    fi
    if [ "$(passwd --status "$child" | awk '{print $2}')" != "L" ]; then
        passwd --lock "$child"
        changed "$child password locked"
    fi
    set_level_group "$child" cairn-l1
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
exec /usr/local/libexec/cairn/cairn-launcher --manifest /usr/local/share/cairn/manifest.json \
    --log-out /usr/local/bin/cairn-log-out --scope-apps "$@"
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

relabel() {
    if command -v restorecon > /dev/null; then
        restorecon -R "$PREFIX/bin/cairn-launcher" "$PREFIX/bin/cairn-session" \
            "$PREFIX/bin/cairn-give-up" "$PREFIX/bin/cairn-log-out" \
            "$PREFIX/libexec/cairn" "$PREFIX/lib64/cairn" "$SHARE" \
            /etc/sddm.conf.d /etc/pam.d/sddm /etc/polkit-1/rules.d /etc/systemd/logind.conf.d \
            /etc/security/faillock.conf
    fi
}

main() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --guardian) guardian="$2"; shift 2 ;;
            --child) child="$2"; shift 2 ;;
            *) usage ;;
        esac
    done
    [ -n "$guardian" ] && [ -n "$child" ] || usage
    [ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }
    check_accounts

    record_facts
    ensure_groups
    ensure_guardian
    ensure_child
    layer_packages
    install_flatpaks
    install_session_files
    install_launcher
    install_display_manager
    install_greeter
    protect_guardians
    relabel

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
