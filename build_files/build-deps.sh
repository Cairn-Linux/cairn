#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Installs what compiling Cairn needs, in the Containerfile's build stage
# only (ADR-0038). Stops the build if Fedora's Qt headers are not exactly
# the Qt the base image ships.

set -ouex pipefail

# The Qt the base ships, read before dnf can change anything.
qt_packages=(qt6-qtbase qt6-qtdeclarative qt6-qtwayland)
declare -A base_version
for package in "${qt_packages[@]}"; do
    base_version[${package}]=$(rpm -q --qf '%{VERSION}-%{RELEASE}' "${package}")
done

dnf5 install -y --setopt=install_weak_deps=False \
    cmake \
    ninja-build \
    gcc-c++ \
    pkgconf-pkg-config \
    python3 \
    dbus-daemon \
    qt6-qtbase-devel \
    qt6-qtdeclarative-devel \
    qt6-qtwayland-devel \
    qt6-qttools-devel \
    qt6-linguist \
    wayland-devel \
    wlr-protocols-devel

# A program built against other Qt headers than the ones it runs with can
# break in ways no test here would see, so a mismatch stops the image.
for package in "${qt_packages[@]}"; do
    devel_version=$(rpm -q --qf '%{VERSION}-%{RELEASE}' "${package}-devel")
    if [[ "${devel_version}" != "${base_version[${package}]}" ]]; then
        echo "ERROR: ${package}-devel is ${devel_version}, but the base image ships ${package} ${base_version[${package}]}." >&2
        echo "ERROR: Cairn must be compiled against the Qt the image runs. Wait for the base or Fedora to catch up." >&2
        exit 1
    fi
done
