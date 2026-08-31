#!/usr/bin/env bash

set -euo pipefail

MODE=valgrind
BUILD_DIR=build
LOG_FILE=
STARTUP_DELAY=3

usage() {
	cat <<'EOF'
Usage: tools/leak-check.sh [--valgrind|--asan] [--build-dir DIR] [--log-file PATH]

Runs gtkterm under a virtual X server, applies a low-noise GTK/GLib runtime
configuration, and exits the app cleanly over D-Bus so leak reports reflect a
normal startup/shutdown cycle instead of a forced timeout.
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--valgrind)
			MODE=valgrind
			shift
			;;
		--asan)
			MODE=asan
			shift
			;;
		--build-dir)
			BUILD_DIR="$2"
			shift 2
			;;
		--log-file)
			LOG_FILE="$2"
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

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BINARY="$ROOT_DIR/$BUILD_DIR/src/gtkterm"
SUPPRESSIONS="$ROOT_DIR/asan_suppressions.txt"

if [[ ! -x "$BINARY" ]]; then
	echo "Binary not found: $BINARY" >&2
	exit 1
fi

if [[ "$MODE" == "valgrind" && ! -f "$SUPPRESSIONS" ]]; then
	echo "Suppressions file not found: $SUPPRESSIONS" >&2
	exit 1
fi

if [[ -z "$LOG_FILE" ]]; then
	LOG_FILE="$ROOT_DIR/${MODE}-leak-check.log"
fi

find_free_display() {
	local display_num
	for display_num in $(seq 90 120); do
		if [[ ! -e "/tmp/.X${display_num}-lock" && ! -S "/tmp/.X11-unix/X${display_num}" ]]; then
			echo ":${display_num}"
			return 0
		fi
	done
	return 1
}

DISPLAY_NUM=$(find_free_display) || {
	echo "Failed to find a free Xvfb display in :90-:120" >&2
	exit 1
}

XVFB_PID=
APP_PID=

cleanup() {
	if [[ -n "$APP_PID" ]] && kill -0 "$APP_PID" 2>/dev/null; then
		gdbus call --session \
			--dest org.gtk.gtkterm \
			--object-path /org/gtk/gtkterm \
			--method org.gtk.Actions.Activate \
			"file-exit" [] {} >/dev/null 2>&1 || true
		wait "$APP_PID" 2>/dev/null || true
	fi
	if [[ -n "$XVFB_PID" ]] && kill -0 "$XVFB_PID" 2>/dev/null; then
		kill "$XVFB_PID" 2>/dev/null || true
		wait "$XVFB_PID" 2>/dev/null || true
	fi
}

trap cleanup EXIT INT TERM

Xvfb "$DISPLAY_NUM" -screen 0 1024x768x24 >/dev/null 2>&1 &
XVFB_PID=$!

export DISPLAY="$DISPLAY_NUM"
export G_DEBUG=gc-friendly
export G_SLICE=always-malloc
export GIO_USE_VFS=local
export NO_AT_BRIDGE=1
export GSK_RENDERER=cairo

case "$MODE" in
	valgrind)
		valgrind \
			--tool=memcheck \
			--leak-check=full \
			--show-leak-kinds=definite,indirect,possible \
			--errors-for-leak-kinds=definite,possible \
			--num-callers=25 \
			--suppressions="$SUPPRESSIONS" \
			--log-file="$LOG_FILE" \
			"$BINARY" &
		APP_PID=$!
		;;
	asan)
		export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0:halt_on_error=0
		export LSAN_OPTIONS=report_objects=1
		"$BINARY" >"$LOG_FILE" 2>&1 &
		APP_PID=$!
		;;
esac

sleep "$STARTUP_DELAY"

gdbus call --session \
	--dest org.gtk.gtkterm \
	--object-path /org/gtk/gtkterm \
	--method org.gtk.Actions.Activate \
	"file-exit" [] {} >/dev/null

wait "$APP_PID"
APP_PID=

echo "Leak check complete. Log written to: $LOG_FILE"#!/usr/bin/env bash

set -euo pipefail

MODE=valgrind
BUILD_DIR=build
LOG_FILE=
STARTUP_DELAY=3

usage() {
	cat <<'EOF'
Usage: tools/leak-check.sh [--valgrind|--asan] [--build-dir DIR] [--log-file PATH]

Runs gtkterm under a virtual X server, applies a low-noise GTK/GLib runtime
configuration, and exits the app cleanly over D-Bus so leak reports reflect a
normal startup/shutdown cycle instead of a forced timeout.
EOF
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--valgrind)
			MODE=valgrind
			shift
			;;
		--asan)
			MODE=asan
			shift
			;;
		--build-dir)
			BUILD_DIR="$2"
			shift 2
			;;
		--log-file)
			LOG_FILE="$2"
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

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BINARY="$ROOT_DIR/$BUILD_DIR/src/gtkterm"
SUPPRESSIONS="$ROOT_DIR/asan_suppressions.txt"

if [[ ! -x "$BINARY" ]]; then
	echo "Binary not found: $BINARY" >&2
	exit 1
fi

if [[ "$MODE" == "valgrind" && ! -f "$SUPPRESSIONS" ]]; then
	echo "Suppressions file not found: $SUPPRESSIONS" >&2
	exit 1
fi

if [[ -z "$LOG_FILE" ]]; then
	LOG_FILE="$ROOT_DIR/${MODE}-leak-check.log"
fi

find_free_display() {
	local display_num
	for display_num in $(seq 90 120); do
		if [[ ! -e "/tmp/.X${display_num}-lock" && ! -S "/tmp/.X11-unix/X${display_num}" ]]; then
			echo ":${display_num}"
			return 0
		fi
	done
	return 1
}

DISPLAY_NUM=$(find_free_display) || {
	echo "Failed to find a free Xvfb display in :90-:120" >&2
	exit 1
}

XVFB_PID=
APP_PID=

cleanup() {
	if [[ -n "$APP_PID" ]] && kill -0 "$APP_PID" 2>/dev/null; then
		gdbus call --session \
			--dest org.gtk.gtkterm \
			--object-path /org/gtk/gtkterm \
			--method org.gtk.Actions.Activate \
			"file-exit" [] {} >/dev/null 2>&1 || true
		wait "$APP_PID" 2>/dev/null || true
	fi
	if [[ -n "$XVFB_PID" ]] && kill -0 "$XVFB_PID" 2>/dev/null; then
		kill "$XVFB_PID" 2>/dev/null || true
		wait "$XVFB_PID" 2>/dev/null || true
	fi
}

trap cleanup EXIT INT TERM

Xvfb "$DISPLAY_NUM" -screen 0 1024x768x24 >/dev/null 2>&1 &
XVFB_PID=$!

export DISPLAY="$DISPLAY_NUM"
export G_DEBUG=gc-friendly
export G_SLICE=always-malloc
export GIO_USE_VFS=local
export NO_AT_BRIDGE=1
export GSK_RENDERER=cairo

case "$MODE" in
	valgrind)
		valgrind \
			--tool=memcheck \
			--leak-check=full \
			--show-leak-kinds=definite,indirect,possible \
			--errors-for-leak-kinds=definite,possible \
			--num-callers=25 \
			--suppressions="$SUPPRESSIONS" \
			--log-file="$LOG_FILE" \
			"$BINARY" &
		APP_PID=$!
		;;
	asan)
		export ASAN_OPTIONS=detect_leaks=1:abort_on_error=0:halt_on_error=0
		export LSAN_OPTIONS=report_objects=1
		"$BINARY" >"$LOG_FILE" 2>&1 &
		APP_PID=$!
		;;
esac

sleep "$STARTUP_DELAY"

gdbus call --session \
	--dest org.gtk.gtkterm \
	--object-path /org/gtk/gtkterm \
	--method org.gtk.Actions.Activate \
	"file-exit" [] {} >/dev/null

wait "$APP_PID"
APP_PID=

echo "Leak check complete. Log written to: $LOG_FILE"