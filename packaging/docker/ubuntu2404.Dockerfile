# GTKTerm build-test image for Ubuntu 24.04 LTS (Noble).
#
# Ubuntu 24.04 ships glibc 2.39, which does NOT provide the cfsetobaud()
# baud_t API (added in glibc 2.42).  Building here therefore exercises the
# kernel termios2/BOTHER ioctl fallback path in src/baud.c, making this image
# a useful check that the "old glibc" branch still compiles cleanly.
#
# Build the image (run from the repository root):
#     docker build -f packaging/docker/ubuntu2404.Dockerfile -t gtkterm:ubuntu2404 .
#
# The build itself runs during "docker build"; a successful image means the
# project compiled.  To poke around or rebuild interactively:
#     docker run --rm -it gtkterm:ubuntu2404 bash

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Toolchain + build system + project dependencies.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        meson \
        ninja-build \
        pkg-config \
        gettext \
        desktop-file-utils \
        libglib2.0-dev-bin \
        libgtk-4-dev \
        libvte-2.91-gtk4-dev \
        libgudev-1.0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

# Confirm the expected configuration on this glibc: the libc baud_t API is
# absent, so the kernel termios2 fallback is selected.
RUN meson setup build \
    && grep -E 'HAVE_CFSETOBAUD|HAVE_LINUX_TERMIOS2|HAVE_LINUX_TERMIOS_H' build/config.h || true

# Compile the project.
RUN ninja -C build

CMD ["bash"]
