#!/usr/bin/env bash

set -euo pipefail

usage() {
	cat <<'EOF'
Usage: packaging/scripts/build-appimage.sh [options]

Builds an AppImage for gtkterm.

Options:
  --build-dir DIR   Meson build directory (default: builddir)
  --output PATH     Output AppImage path (default: gtkterm-<arch>.AppImage)
  -h, --help        Show this help text
EOF
}

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
BUILD_DIR="$ROOT_DIR/builddir"
APPDIR_BASE="$ROOT_DIR/.appimage"
APPDIR="$APPDIR_BASE/AppDir"
ARCH=$(uname -m)
OUTPUT_PATH="$ROOT_DIR/gtkterm-${ARCH}.AppImage"

while [[ $# -gt 0 ]]; do
	case "$1" in
		--build-dir)
			BUILD_DIR="$2"
			shift 2
			;;
		--output)
			OUTPUT_PATH="$2"
			shift 2
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			echo "Unknown option: $1" >&2
			usage >&2
			exit 2
			;;
	esac
done

if ! command -v linuxdeploy >/dev/null 2>&1; then
	echo "linuxdeploy is not installed. Install it first, then retry." >&2
	exit 1
fi

if ! command -v appimagetool >/dev/null 2>&1; then
	echo "appimagetool is not installed. Install it first, then retry." >&2
	exit 1
fi

if [[ ! -d "$BUILD_DIR" ]]; then
	echo "Build directory not found: $BUILD_DIR" >&2
	echo "Run meson setup first or pass --build-dir." >&2
	exit 1
fi

rm -rf "$APPDIR"
mkdir -p "$APPDIR"

DESTDIR="$APPDIR" ninja -C "$BUILD_DIR" install

BINARY="$APPDIR/usr/bin/gtkterm"
DESKTOP_FILE="$APPDIR/usr/share/applications/org.gtk.gtkterm.desktop"

if [[ ! -x "$BINARY" ]]; then
	echo "Installed binary not found in AppDir: $BINARY" >&2
	exit 1
fi

if [[ ! -f "$DESKTOP_FILE" ]]; then
	echo "Desktop file not found in AppDir: $DESKTOP_FILE" >&2
	exit 1
fi

ICON_FILE=$(find "$APPDIR/usr/share/icons" -type f \( -name 'org.gtk.gtkterm.png' -o -name 'gtkterm.png' \) | head -n 1 || true)

LINUXDEPLOY_CMD=(
	linuxdeploy
	--appdir "$APPDIR"
	--executable "$BINARY"
	--desktop-file "$DESKTOP_FILE"
)

if [[ -n "$ICON_FILE" ]]; then
	LINUXDEPLOY_CMD+=(--icon-file "$ICON_FILE")
fi

"${LINUXDEPLOY_CMD[@]}"

export ARCH
appimagetool "$APPDIR" "$OUTPUT_PATH"

echo "AppImage build complete. Artifact written to: $OUTPUT_PATH"
