# SPDX-License-Identifier: Apache-2.0
# Plasma keeps a cache of the icons it has drawn, and every file in the
# image has the same time, so it never notices that an update changed an
# icon. The image records a fingerprint of its icons; when it differs
# from the one this account last saw, the cache goes and Plasma redraws
# from the files (ADR-0038, slice 5). Plasma sources this at login.

cairn_cache="${XDG_CACHE_HOME:-$HOME/.cache}"
cairn_sum=$(cat /usr/share/cairn/icons.sum 2>/dev/null)
if [ -n "$cairn_sum" ] && [ -d "$cairn_cache" ] &&
    [ "$cairn_sum" != "$(cat "$cairn_cache/cairn-icons.sum" 2>/dev/null)" ]; then
    rm -f "$cairn_cache/icon-cache.kcache"
    printf '%s\n' "$cairn_sum" >"$cairn_cache/cairn-icons.sum"
fi
unset cairn_cache cairn_sum
