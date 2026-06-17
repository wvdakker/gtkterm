#!/usr/bin/env bash

set -euo pipefail

usage() {
	cat <<'EOF'
Usage: packaging/scripts/build-deb.sh [dpkg-buildpackage args]

Builds a Debian package for gtkterm.

This helper keeps Debian metadata in packaging/debian and creates a temporary
top-level debian symlink only for the duration of the build.

Any arguments are passed through to dpkg-buildpackage.

Examples:
  bash packaging/scripts/build-deb.sh
  bash packaging/scripts/build-deb.sh -S
EOF
}

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
PACKAGING_DEBIAN_DIR="$ROOT_DIR/packaging/debian"
TOPLEVEL_DEBIAN_DIR="$ROOT_DIR/debian"
CREATED_LINK=false

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
	exit 0
fi

if ! command -v dpkg-buildpackage >/dev/null 2>&1; then
	echo "dpkg-buildpackage is not installed. Install it first, then retry." >&2
	exit 1
fi

if [[ ! -d "$PACKAGING_DEBIAN_DIR" ]]; then
	echo "Missing Debian metadata directory: $PACKAGING_DEBIAN_DIR" >&2
	exit 1
fi

if [[ -e "$TOPLEVEL_DEBIAN_DIR" && ! -L "$TOPLEVEL_DEBIAN_DIR" ]]; then
	echo "Top-level debian path exists and is not a symlink: $TOPLEVEL_DEBIAN_DIR" >&2
	exit 1
fi

if [[ ! -e "$TOPLEVEL_DEBIAN_DIR" ]]; then
	ln -s packaging/debian "$TOPLEVEL_DEBIAN_DIR"
	CREATED_LINK=true
fi

cleanup() {
	if [[ "$CREATED_LINK" == "true" && -L "$TOPLEVEL_DEBIAN_DIR" ]]; then
		rm -f "$TOPLEVEL_DEBIAN_DIR"
	fi
}

trap cleanup EXIT INT TERM

pushd "$ROOT_DIR" >/dev/null
DEB_BUILD_OPTIONS=nocheck dpkg-buildpackage -us -uc -b "$@"

echo "Debian package build complete. Artifacts are in: $ROOT_DIR"
