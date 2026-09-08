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
set -ouex pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX=/usr/local
SHARE="$PREFIX/share/cairn"
FACTS=/var/lib/cairn/facts.txt
LEVEL_GROUPS=(cairn-l1 cairn-l2 cairn-l3 cairn-l4 cairn-guardian)
LAYERED_PACKAGES=(labwc scummvm sddm sddm-breeze sddm-wayland-plasma)
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

while [ $# -gt 0 ]; do
    case "$1" in
        --guardian) guardian="$2"; shift 2 ;;
        --child) child="$2"; shift 2 ;;
        *) usage ;;
    esac
done
[ -n "$guardian" ] && [ -n "$child" ] || usage
[ "$(id -u)" -eq 0 ] || { echo "run as root" >&2; exit 1; }

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

# Exactly one level group per account: add the wanted one, drop the others.
set_level_group() {
    local user="$1" wanted="$2" group
    for group in "${LEVEL_GROUPS[@]}"; do
        if id -nG "$user" | tr ' ' '\n' | grep -qx "$group"; then
            if [ "$group" != "$wanted" ]; then
                gpasswd -d "$user" "$group"
                changed "$user left $group"
            fi
        elif [ "$group" = "$wanted" ]; then
            gpasswd -a "$user" "$group"
            changed "$user joined $group"
        fi
    done
}

# --- 3. Accounts ---------------------------------------------------------------
ensure_guardian() {
    if ! id "$guardian" > /dev/null 2>&1; then
        useradd --create-home --groups wheel "$guardian"
        changed "guardian account $guardian"
        if [ -n "${CAIRN_GUARDIAN_PASSWORD:-}" ]; then
            echo "$guardian:$CAIRN_GUARDIAN_PASSWORD" | chpasswd
        else
            echo "note: $guardian has no password yet; set one with: passwd $guardian"
        fi
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
package_present() {
    rpm -q "$1" > /dev/null 2>&1 \
        || rpm-ostree status | grep -E '^\s*LayeredPackages:' | tr ' ' '\n' | grep -qx "$1"
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
install_file() {
    local mode="$1" source="$2" target="$3"
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
    install_file 644 "$REPO/session/sessions/cairn.desktop" "$SHARE/sessions/cairn.desktop"
    # What a child's session may ask the system to do (issue #35). polkitd
    # watches the directory, so the rule applies without a restart.
    install_file 644 "$REPO/session/polkit/rules.d/10-cairn-levels.rules" /etc/polkit-1/rules.d/10-cairn-levels.rules
}

# --- 7. The display manager: SDDM, swapped in explicitly (ADR-0016) ---------
install_display_manager() {
    install_file 644 "$REPO/session/sddm/sddm.conf.d/10-cairn.conf" /etc/sddm.conf.d/10-cairn.conf
    install_file 644 "$REPO/session/sddm/sddm.conf.d/zz-cairn-no-autologin.conf" /etc/sddm.conf.d/zz-cairn-no-autologin.conf
    install_file 644 "$REPO/session/sddm/pam.d/sddm" /etc/pam.d/sddm
    # Guardians are hidden from the greeter's tiles. The list changes with
    # the accounts, so it is generated here rather than shipped.
    local guardians
    guardians="$(getent group cairn-guardian | cut -d: -f4)"
    printf '# Written by provision/cairn-provision.sh; the members of cairn-guardian.\n[Users]\nHideUsers=%s\n' \
        "$guardians" > /tmp/cairn-guardians.conf
    install_file 644 /tmp/cairn-guardians.conf /etc/sddm.conf.d/20-cairn-guardians.conf
    rm /tmp/cairn-guardians.conf
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
    cat > /tmp/cairn-launcher.wrapper <<'WRAPPER'
#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Installed by provision/cairn-provision.sh; not the Phase 1 packaging.
export LD_LIBRARY_PATH=/usr/local/lib64/cairn
exec /usr/local/libexec/cairn/cairn-launcher --manifest /usr/local/share/cairn/manifest.json "$@"
WRAPPER
    install_file 755 /tmp/cairn-launcher.wrapper "$PREFIX/bin/cairn-launcher"
    rm /tmp/cairn-launcher.wrapper
}

relabel() {
    if command -v restorecon > /dev/null; then
        restorecon -R "$PREFIX/bin/cairn-launcher" "$PREFIX/bin/cairn-session" \
            "$PREFIX/libexec/cairn" "$PREFIX/lib64/cairn" "$SHARE" \
            /etc/sddm.conf.d /etc/pam.d/sddm /etc/polkit-1/rules.d
    fi
}

record_facts
ensure_groups
ensure_guardian
ensure_child
layer_packages
install_flatpaks
install_session_files
install_launcher
install_display_manager
relabel

set +x
echo "done: $changes change(s)"
if [ "$reboot_needed" = yes ]; then
    echo "reboot: layered packages or the display manager change take effect at the next boot"
fi
