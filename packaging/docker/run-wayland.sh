#!/bin/sh -e
#
# Run the containerised GTKTerm GUI on the host's Wayland session.
#
# Usage (from anywhere):
#     packaging/docker/run-wayland.sh [gtkterm args...]
#
# Examples:
#     packaging/docker/run-wayland.sh
#     packaging/docker/run-wayland.sh --speed 115200 --port /dev/ttyUSB0
#     SERIAL_DEVICE=/dev/ttyUSB0 packaging/docker/run-wayland.sh -p /dev/ttyUSB0
#
# Environment overrides:
#     IMAGE          container image to run   (default: gtkterm:ubuntu2404)
#     SERIAL_DEVICE  host serial device to expose to the container (optional)
#
# Notes:
#   * Requires a running Wayland session (XDG_RUNTIME_DIR + WAYLAND_DISPLAY).
#   * GSK_RENDERER=cairo forces GTK4's software renderer, so no GPU drivers
#     are needed inside the image.
#   * SERIAL_DEVICE is passed in with --device and its group is granted to the
#     container (--group-add) so the non-root container user can read/write it.
#     If you lack permission on the host, the script reports it and exits.

IMAGE="${IMAGE:-gtkterm:ubuntu2404}"
: "${XDG_RUNTIME_DIR:?No Wayland session: XDG_RUNTIME_DIR is unset}"
: "${WAYLAND_DISPLAY:=wayland-0}"

# Optionally expose a host serial device to the container.
DEVICE_OPTS=
if [ -n "${SERIAL_DEVICE:-}" ]; then
	if [ ! -e "$SERIAL_DEVICE" ]; then
		echo "run-wayland.sh: serial device '$SERIAL_DEVICE' does not exist" >&2
		exit 1
	fi
	if [ ! -c "$SERIAL_DEVICE" ]; then
		echo "run-wayland.sh: '$SERIAL_DEVICE' is not a character device" >&2
		exit 1
	fi
	if [ ! -r "$SERIAL_DEVICE" ] || [ ! -w "$SERIAL_DEVICE" ]; then
		dev_owner=$(stat -c '%U:%G' "$SERIAL_DEVICE" 2>/dev/null)
		dev_mode=$(stat -c '%a'    "$SERIAL_DEVICE" 2>/dev/null)
		dev_group=$(stat -c '%G'   "$SERIAL_DEVICE" 2>/dev/null)
		echo "run-wayland.sh: no permission to access '$SERIAL_DEVICE'" >&2
		echo "    (owned by $dev_owner, mode $dev_mode; you are $(id -un))" >&2
		echo "    Add yourself to the '$dev_group' group, then log out and back in:" >&2
		echo "        sudo usermod -aG $dev_group $(id -un)" >&2
		exit 1
	fi
	DEVICE_OPTS="--device $SERIAL_DEVICE --group-add $(stat -c %g "$SERIAL_DEVICE")"
fi

# DEVICE_OPTS is intentionally left unquoted so it splits into separate args
# (serial device paths and numeric gids never contain whitespace).
exec docker run --rm \
	--init \
	--user "$(id -u):$(id -g)" \
	-e XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
	-e WAYLAND_DISPLAY="$WAYLAND_DISPLAY" \
	-e GDK_BACKEND=wayland \
	-e GSK_RENDERER=cairo \
	-v "$XDG_RUNTIME_DIR:$XDG_RUNTIME_DIR" \
	$DEVICE_OPTS \
	"$IMAGE" /src/build/src/gtkterm "$@"
