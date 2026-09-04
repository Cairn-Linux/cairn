# Cairn Linux — bootc image.
# Layout follows ublue-os/image-template so its Justfile and workflows can be
# adopted unchanged in Phase 1. NOT BUILT DURING PHASE 0 (see docs/ROADMAP.md).

# Build context: scripts and files to lay over the base image.
FROM scratch AS ctx
COPY build_files /
COPY system_files /system_files

# Base image: Bazzite, KDE variant (ADR-0006). A Universal Blue image derived
# from Fedora; ships Plasma, the Steam client and Proton plumbing that Phase 1
# needs (ADR-0004). One Intel/AMD image in v1; no Nvidia variants. Pin to a
# digest once CI exists (P1-3). Compliance work before any image is published
# is P1-14 (docs/research/base-image-policy.md). Fallback: kinoite-main.
FROM ghcr.io/ublue-os/bazzite:stable

RUN --mount=type=bind,from=ctx,source=/,target=/ctx \
    --mount=type=cache,dst=/var/cache \
    --mount=type=cache,dst=/var/log \
    --mount=type=tmpfs,dst=/tmp \
    /ctx/build.sh

RUN bootc container lint
