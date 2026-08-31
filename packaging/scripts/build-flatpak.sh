#!/usr/bin/env bash

set -euo pipefail

usage() {
	cat <<'EOF'
Usage: packaging/scripts/build-flatpak.sh [flatpak-builder args]

Builds a local Flatpak bundle for gtkterm.

Any arguments are passed through to flatpak-builder.

Examples:
  bash packaging/scripts/build-flatpak.sh
  bash packaging/scripts/build-flatpak.sh --disable-rofiles-fuse
EOF
}

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
MANIFEST="$ROOT_DIR/packaging/flatpak/org.gtk.gtkterm.json"
BUILD_DIR="$ROOT_DIR/.flatpak-builder/build"
REPO_DIR="$ROOT_DIR/.flatpak-repo"
BUNDLE_PATH="$ROOT_DIR/gtkterm.flatpak"
BUILDER_ARGS=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		-h|--help)
			usage
			exit 0
			;;
		*)
			BUILDER_ARGS+=("$1")
			shift
			;;
	esac
done

if ! command -v flatpak-builder >/dev/null 2>&1; then
	echo "flatpak-builder is not installed. Install it first, then retry." >&2
	exit 1
fi

if ! command -v flatpak >/dev/null 2>&1; then
	echo "flatpak is not installed. Install it first, then retry." >&2
	exit 1
fi

if [[ ! -f "$MANIFEST" ]]; then
	echo "Missing Flatpak manifest: $MANIFEST" >&2
	exit 1
fi

mkdir -p "$BUILD_DIR" "$REPO_DIR"

flatpak-builder \
	--force-clean \
	--repo="$REPO_DIR" \
	"${BUILDER_ARGS[@]}" \
	"$BUILD_DIR" \
	"$MANIFEST"

flatpak build-bundle "$REPO_DIR" "$BUNDLE_PATH" org.gtk.gtkterm stable

echo "Flatpak build complete. Bundle written to: $BUNDLE_PATH"
