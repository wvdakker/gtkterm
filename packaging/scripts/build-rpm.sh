#!/usr/bin/env bash

set -euo pipefail

usage() {
	cat <<'EOF'
Usage: packaging/scripts/build-rpm.sh [rpmbuild args]

Builds RPM packages for gtkterm.

Any arguments are passed through to rpmbuild.

Examples:
  bash packaging/scripts/build-rpm.sh
  bash packaging/scripts/build-rpm.sh --nocheck
EOF
}

ROOT_DIR=$(cd "$(dirname "$0")/../.." && pwd)
SPEC_FILE="$ROOT_DIR/packaging/rpm/gtkterm.spec"
RPM_ROOT="$ROOT_DIR/.rpmbuild"
SOURCES_DIR="$RPM_ROOT/SOURCES"
BUILD_ROOT="$RPM_ROOT/BUILD"
RPMS_DIR="$RPM_ROOT/RPMS"
SRPMS_DIR="$RPM_ROOT/SRPMS"
SPECS_DIR="$RPM_ROOT/SPECS"
ARCHIVE_NAME="gtkterm-2.0.0.tar.gz"
ARCHIVE_PATH="$SOURCES_DIR/$ARCHIVE_NAME"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
	exit 0
fi

if ! command -v rpmbuild >/dev/null 2>&1; then
	echo "rpmbuild is not installed. Install rpm-build first, then retry." >&2
	exit 1
fi

if [[ ! -f "$SPEC_FILE" ]]; then
	echo "Missing RPM spec file: $SPEC_FILE" >&2
	exit 1
fi

mkdir -p "$SOURCES_DIR" "$BUILD_ROOT" "$RPMS_DIR" "$SRPMS_DIR" "$SPECS_DIR"

cp "$SPEC_FILE" "$SPECS_DIR/gtkterm.spec"

git -C "$ROOT_DIR" archive \
	--format=tar.gz \
	--prefix=gtkterm-2.0.0/ \
	HEAD > "$ARCHIVE_PATH"

rpmbuild \
	--define "_topdir $RPM_ROOT" \
	-ba "$SPECS_DIR/gtkterm.spec" \
	"$@"

echo "RPM build complete. Artifacts are in: $RPMS_DIR and $SRPMS_DIR"
