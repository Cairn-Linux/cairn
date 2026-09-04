# Cairn Linux — bootc image.
# Layout follows ublue-os/image-template so its Justfile and workflows can be
# adopted unchanged in Phase 1. NOT BUILT DURING PHASE 0 (see docs/ROADMAP.md).

# Build context: scripts and files to lay over the base image.
FROM scratch AS ctx
COPY build_files /
COPY system_files /system_files

# Base image: ROADMAP decision D4. Bazzite (KDE) is the recommendation because
# Phases 2–3 need Plasma, Steam and Proton plumbing. Pin to a digest once CI
# exists (P1-3). Alternative if footprint or trademark policy is a problem:
#   ghcr.io/ublue-os/kinoite-main:latest
FROM ghcr.io/ublue-os/bazzite:stable

RUN --mount=type=bind,from=ctx,source=/,target=/ctx \
    --mount=type=cache,dst=/var/cache \
    --mount=type=cache,dst=/var/log \
    --mount=type=tmpfs,dst=/tmp \
    /ctx/build.sh

RUN bootc container lint
