#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
#
# Builds Cairn's installer ISO from the image in your own podman storage
# (ADR-0038, slice 6). Run it from your own account with sudo:
#
#   sudo tools/build-iso.sh                    # disk_config/iso.toml
#   sudo tools/build-iso.sh /path/to/test.toml # a private test config
#
# bootc-image-builder needs rootful podman, so this asks for root once and
# does every step itself. It stands in for image-template's
# `just build-iso` until P1-3 adopts that Justfile, and runs
# bootc-image-builder with the same flags. docs/DEVELOPMENT.md,
# "Building the installer", says what it prints and how long it takes.

set -ouex pipefail

if [[ ${EUID} -ne 0 || -z ${SUDO_USER:-} || ${SUDO_USER} == root ]]; then
    echo "Run this with sudo from your own account: sudo tools/build-iso.sh [config.toml]" >&2
    exit 1
fi

checkout=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "${checkout}"

# IMAGE_NAME and BIB_IMAGE, the names image-template's Justfile reads.
# shellcheck source=cairn.env
source ./cairn.env
image="localhost/${IMAGE_NAME}:dev"

config=$(realpath "${1:-disk_config/iso.toml}")
if [[ ! -r ${config} ]]; then
    echo "There is no config at ${config}." >&2
    exit 1
fi

# A config other than Cairn's own can hold a password and a key. Its ISO
# goes in a folder of its own, so it is never taken for the family's.
if [[ ${config} == "${checkout}/disk_config/iso.toml" ]]; then
    out="${checkout}/output"
else
    out="${checkout}/output/test"
fi

user_uid=${SUDO_UID}
user_gid=${SUDO_GID}
user_home=$(getent passwd "${SUDO_USER}" | cut -d: -f6)
if [[ ! -d /run/user/${user_uid} ]]; then
    echo "${SUDO_USER} has no login session, so their podman storage cannot be read. Log in as ${SUDO_USER} and run this again." >&2
    exit 1
fi

# Runs a command as the person who ran sudo, with the environment their
# own rootless podman needs and nothing else.
as_user() {
    runuser -u "${SUDO_USER}" -- env -i PATH=/usr/bin:/bin \
        HOME="${user_home}" XDG_RUNTIME_DIR="/run/user/${user_uid}" "$@"
}

as_user mkdir -p "${out}"
as_user touch "${out}/build-iso.log"
exec > >(tee "${out}/build-iso.log") 2>&1

# Copy the image into root's storage, where bootc-image-builder reads it.
# An image is named by the digest of its configuration, which save and
# load keep, so equal names mean root already has this exact image.
user_id=$(as_user podman image inspect --format '{{.Id}}' "${image}")
root_id=$(podman image inspect --format '{{.Id}}' "${image}" 2>/dev/null || true)
if [[ ${root_id} != "${user_id}" ]]; then
    as_user podman save "${image}" | podman load
    root_id=$(podman image inspect --format '{{.Id}}' "${image}")
fi
if [[ ${root_id} != "${user_id}" ]]; then
    echo "Root's copy of ${image} is not ${SUDO_USER}'s. Nothing was built." >&2
    exit 1
fi

work=$(as_user mktemp -d -p "${out}" .bib.XXXXXXXXXX)
keys=$(as_user mktemp -d -p "${out}" .keys.XXXXXXXXXX)
trap 'rm -rf "${work}" "${keys}"' EXIT

# bootc-image-builder reads the image's own repositories to pick the
# installer's packages. It looks for a repository's signing key in its own
# container, not in the image, when the repository checks signed metadata,
# as Bazzite's terra-mesa does. So the image's keys are lent to it.
cid=$(podman create "${image}" true)
podman cp "${cid}:/etc/pki/rpm-gpg/." "${keys}/"
podman rm "${cid}"

# The same flags as image-template's _build-bib recipe, without -it, so it
# also runs where there is no terminal. anaconda-iso is the type its
# build-disk workflow names; the Justfile's "iso" is the same type.
# --config is named, because without a config bootc-image-builder quietly
# makes an installer that wipes every disk without asking.
podman run --rm --privileged --pull=missing --net=host \
    --security-opt label=type:unconfined_t \
    -v "${config}:/config.toml:ro" \
    -v "${work}:/output" \
    -v "${keys}:/etc/pki/rpm-gpg:ro" \
    -v /var/lib/containers/storage:/var/lib/containers/storage \
    "${BIB_IMAGE}" \
    build --type anaconda-iso --use-librepo=True --rootfs=btrfs \
    --config /config.toml \
    "${image}"

rm -rf "${out}/bootiso" "${out}/manifest-anaconda-iso.json"
mv "${work}"/* "${out}/"
chown -R "${user_uid}:${user_gid}" "${out}/bootiso" "${out}/manifest-anaconda-iso.json"

cd "${out}/bootiso"
as_user sha256sum install.iso | as_user tee install.iso.sha256
set +x
echo
echo "The installer is ${out}/bootiso/install.iso, $(du -h install.iso | cut -f1)."
echo "Built from ${image} (${user_id:0:12}) with ${config}."
