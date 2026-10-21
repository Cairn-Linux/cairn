#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Compiles and tests Cairn in the Containerfile's build stage, then installs
# it into /cairn-root, which the image copies over / (ADR-0038). A failing
# test stops the build, so a broken Cairn never becomes an image. A test
# that needs what a container build lacks (user namespaces, a systemd user
# manager, a folder that can reflink) skips itself here. The log lists
# those skips, and `ctest` on the host still has to pass before an image is
# built.

set -ouex pipefail

# Cairn is compiled and tested as an ordinary user, as on a developer's
# machine. Some tests check what a person without root may not read, and
# provisioning's copy step refuses to run as root at all.
useradd --system --no-create-home --shell /sbin/nologin cairn-build
install -d -o cairn-build -g cairn-build /build

# COPY keeps the checkout's file modes. A clone made with a strict umask
# has files only their owner may read, and that owner is now root.
chmod -R a+rX /src

# setpriv, not runuser or su: there is no PAM session in a container build.
as_builder() {
    setpriv --reuid=cairn-build --regid=cairn-build --init-groups \
        env HOME=/build "$@"
}

as_builder cmake -S /src -B /build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
as_builder cmake --build /build
as_builder env QT_QPA_PLATFORM=offscreen \
    ctest --test-dir /build --verbose --no-tests=error 2>&1 |
    tee /tmp/ctest.log

# ctest counts a test that skipped part of itself as passed, so name the
# skips where the person building will see them.
echo "Tests skipped in the build stage; run ctest on the host to cover them:"
grep -E 'skipped=|: SKIP ' /tmp/ctest.log || echo "(none)"

# Root installs, so every file in the tree is owned by root.
DESTDIR=/cairn-root cmake --install /build
