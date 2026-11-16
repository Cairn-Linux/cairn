#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Names Cairn's version in os-release as IMAGE_VERSION, when whoever
# builds the image gives one with `--build-arg CAIRN_VERSION=...`
# (docs/DEVELOPMENT.md, "Building the image"). Family's About page and
# `hostnamectl` show it. The Containerfile runs it in its own step after
# build.sh, with the version as its one argument, so only that step and
# the ones after it run again when the version changes.
# build_files/check-image-version.sh checks the result.

set -ouex pipefail

# [a-z] means the 26 letters only in the C locale.
export LC_ALL=C

version=$1

# No version given: no IMAGE_VERSION line, and nothing made up in its
# place.
if [[ -z "${version}" ]]; then
    exit 0
fi

# os-release(5) allows only these in IMAGE_VERSION.
if [[ ! "${version}" =~ ^[a-z0-9._-]+$ ]]; then
    echo "CAIRN_VERSION is '${version}', but it may hold only lower-case letters, digits, '.', '_' and '-'." >&2
    exit 1
fi

# name-and-logos.sh writes os-release whole, with no IMAGE_VERSION.
# install writes a new file with the mode it is given, as there.
cp /usr/lib/os-release /tmp/os-release
printf 'IMAGE_VERSION=%s\n' "${version}" >>/tmp/os-release
install -m 0644 /tmp/os-release /usr/lib/os-release
