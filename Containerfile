# SPDX-License-Identifier: Apache-2.0
# Cairn Linux — bootc image (ADR-0038).
# Layout follows ublue-os/image-template so its Justfile and workflows can be
# adopted unchanged later in P1-3. Built on the dev PC for now; see
# docs/DEVELOPMENT.md, "Building the image".

# Base image: Bazzite, KDE variant (ADR-0006). A Universal Blue image derived
# from Fedora; ships Plasma, the Steam client and Proton plumbing that Phase 1
# needs (ADR-0004). One Intel/AMD image in v1; no Nvidia variants. Pin to a
# digest once CI exists (P1-3). Compliance work before any image is published
# is P1-14 (docs/research/base-image-policy.md). Fallback: kinoite-main.
# The build stage and the image both start from this one name, so Cairn is
# always compiled against the Qt the image ships.
ARG BASE_IMAGE=ghcr.io/ublue-os/bazzite:stable

# Build context: the scripts the image's stage runs, and system_files/ to
# lay over the image.
FROM scratch AS ctx
COPY build_files /
COPY system_files /system_files

# packages.sh alone, so editing build.sh or system_files/ keeps the cached
# packages layer.
FROM scratch AS packages
COPY build_files/packages.sh /

# Compile Cairn. This stage is thrown away: its compiler, the -devel packages
# and the systemd update they pull in never reach the image. Only the folder
# `cmake --install` fills is kept.
FROM ${BASE_IMAGE} AS build

# Each step copies only the files it reads, so editing a doc or another
# script keeps the cached build tools and the compiled tree.
COPY build_files/build-deps.sh /cairn-build/
RUN --mount=type=cache,dst=/var/cache \
    --mount=type=tmpfs,dst=/tmp \
    /cairn-build/build-deps.sh

COPY CMakeLists.txt CMakePresets.json /src/
COPY cmake /src/cmake
COPY brand /src/brand
COPY external /src/external
COPY launcher /src/launcher
COPY greeter /src/greeter
COPY guardian /src/guardian
COPY session /src/session
COPY provision /src/provision
COPY tools /src/tools

COPY build_files/build-cairn.sh /cairn-build/
RUN --mount=type=tmpfs,dst=/tmp \
    /cairn-build/build-cairn.sh

# The image.
FROM ${BASE_IMAGE}

# Packages first, so that no RPM puts its own file over one of Cairn's.
RUN --mount=type=bind,from=packages,source=/,target=/ctx \
    --mount=type=cache,dst=/var/cache \
    --mount=type=cache,dst=/var/log \
    --mount=type=tmpfs,dst=/tmp \
    /ctx/packages.sh

COPY --from=build /cairn-root/ /

RUN --mount=type=bind,from=ctx,source=/,target=/ctx \
    --mount=type=cache,dst=/var/cache \
    --mount=type=cache,dst=/var/log \
    --mount=type=tmpfs,dst=/tmp \
    /ctx/build.sh

# Any lint warning stops the build.
RUN bootc container lint --fatal-warnings
