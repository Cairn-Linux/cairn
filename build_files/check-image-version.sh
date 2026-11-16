#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Checks that os-release names the version the build was given, once,
# in the form os-release(5) allows and no longer than Family's About
# page (#308) shows, and names none when the build was given none. So
# a doubled line, a bad value or a line written with no version given
# stops the build. Runs in the Containerfile's check step with the same
# CAIRN_VERSION as image-version.sh. It cannot catch an older version
# podman kept from its cache, because then this step comes from the
# cache too; docs/DEVELOPMENT.md says how to look after the build.

set -ouex pipefail

# [a-z] means the 26 letters only in the C locale.
export LC_ALL=C

version=$1
# grep -c prints 0 and fails when there is no such line.
lines=$(grep -c '^IMAGE_VERSION=' /usr/lib/os-release || true)

if [[ -z "${version}" ]]; then
    if [[ "${lines}" != 0 ]]; then
        echo "os-release names a version, but the build was given none." >&2
        exit 1
    fi
    exit 0
fi

if [[ "${lines}" != 1 ]]; then
    echo "os-release has ${lines} IMAGE_VERSION lines, but it must have one." >&2
    exit 1
fi

# The line as written. image-version.sh writes the value bare, and
# os-release(5) allows no other characters in it. 64 is the most
# Family's About page shows.
found=$(sed -n 's/^IMAGE_VERSION=//p' /usr/lib/os-release)
if [[ ! "${found}" =~ ^[a-z0-9._-]{1,64}$ ]]; then
    echo "os-release has IMAGE_VERSION=${found}, but only lower-case letters, digits, '.', '_' and '-', at most 64 of them, may follow the '='." >&2
    exit 1
fi
if [[ "${found}" != "${version}" ]]; then
    echo "os-release's IMAGE_VERSION is '${found}', but the build was given '${version}'." >&2
    exit 1
fi
