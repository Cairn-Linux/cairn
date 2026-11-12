#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Stops rpm-ostree's "count me" report (DESIGN §9.3: no telemetry).
# build.sh runs it, and build_files/check-no-countme.sh checks the result.
#
# Fedora's 90-default.preset switches rpm-ostree-countme.timer on, through
# a link in timers.target.wants that no package owns. Within a day of
# start, and every few days after that, the timer's service sent a request
# to every repository whose file says countme=1, today Fedora's and
# Terra's, with a user agent naming the computer's system and release.
#
# Masking is the opt-out rpm-ostree's own page gives
# (https://coreos.github.io/rpm-ostree/countme/). The service is masked as
# well, so starting it by hand sends nothing either. systemctl preset-all
# leaves a masked unit alone, should it ever run. Universal Blue's images
# keep the report on, so there is no upstream switch to inherit; Bazzite's
# own build masks the units it does not want in the same way.

set -ouex pipefail

# disable first, so the link the preset made goes and nothing points at
# the masked timer. It fails if the base drops the timer, which stops the
# build here until the change is looked at.
systemctl disable rpm-ostree-countme.timer
systemctl mask rpm-ostree-countme.timer rpm-ostree-countme.service

# The repositories' own countme=1 is the second way out: `rpm-ostree
# countme` run by hand, and the metadata downloads of rpm-ostree and dnf5,
# which add the weekly count to a repository that asks for it. countme=0
# turns that off for both. dnf5's repos.override.d would not do, because
# rpm-ostree does not read it.
find /usr/share/dnf5/repos.d /etc/yum.repos.d -name '*.repo' \
    -exec sed -i -E 's/^[[:space:]]*countme[[:space:]]*=.*$/countme=0/' {} +
