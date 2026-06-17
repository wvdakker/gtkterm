#!/usr/bin/env bash

set -euo pipefail

usage() {
	cat <<'EOF'
Usage: packaging/scripts/build-snap.sh [snapcraft args]

Builds a Snap package for gtkterm from this repository.

Options:
  --use-lxd     Use Snapcraft's default managed provider (omit --destructive-mode).
  -h, --help    Show this help text.

Any other arguments are passed through to snapcraft.

Examples:
  bash packaging/scripts/build-snap.sh
  bash packaging/scripts/build-snap.sh --use-lxd
  bash packaging/scripts/build-snap.sh --verbose
EOF
}

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
SNAPCRAFT_FILE="$ROOT_DIR/packaging/snap/snapcraft.yaml"
USE_DESTRUCTIVE_MODE=true
SNAPCRAFT_ARGS=()

while [[ $# -gt 0 ]]; do
	case "$1" in
		--use-lxd)
			USE_DESTRUCTIVE_MODE=false
			shift
			;;
		-h|--help)
			usage
			exit 0
			;;
		*)
			SNAPCRAFT_ARGS+=("$1")
			shift
			;;
	esac
done

if ! command -v snapcraft >/dev/null 2>&1; then
	echo "snapcraft is not installed. Install it first, then retry." >&2
	exit 1
fi

if [[ ! -f "$SNAPCRAFT_FILE" ]]; then
	echo "Missing snapcraft manifest: $SNAPCRAFT_FILE" >&2
	exit 1
fi

pushd "$ROOT_DIR" >/dev/null

if [[ "$USE_DESTRUCTIVE_MODE" == "true" ]]; then
	snapcraft --destructive-mode --file "$SNAPCRAFT_FILE" "${SNAPCRAFT_ARGS[@]}"
else
	snapcraft --file "$SNAPCRAFT_FILE" "${SNAPCRAFT_ARGS[@]}"
fi

echo "Snap build complete. Artifact should be in: $ROOT_DIR"
