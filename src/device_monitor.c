/***********************************************************************/
/* device_monitor.c                                                    */
/* ----------------                                                    */
/*                      GTKTerm Software                               */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Monitor device to autoreconnect                                */
/*                                                                     */
/*   Written by Kevin Picot - picotk27@gmail.com                       */
/*                                                                     */
/***********************************************************************/

#include <device_monitor.h>
#include <stdbool.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gudev/gudev.h>

#include "serial.h"
#include "interface.h"
#include "term_config.h"
#include "config_file.h"

/* True when the port was closed because the system is going to sleep,
 * so we know to reopen it on resume regardless of autoreconnect_enabled. */
static gboolean suspended_while_open = FALSE;

static GUdevClient *udev_client = NULL;
static GDBusConnection *system_bus = NULL;

static inline void device_monitor_status(const bool connected)
{
	if (connected) {
		if (term_conf.autoreconnect_enabled || suspended_while_open) {
			interface_open_port();
		}
	} else
		interface_close_port();
}

static inline void device_monitor_handle(const char *action)
{
	if (strcmp(action, "remove") == 0)
		device_monitor_status(false);
	else if (strcmp(action, "add") == 0)
		device_monitor_status(true);
}

static void event_udev(GUdevClient *client G_GNUC_UNUSED, const gchar *action, GUdevDevice *device)
{
	const gchar *name;

	if (!device || !action)
		return;

	if (!g_udev_device_get_device_file(device))
		return;

	name = config.port;

	if (strcmp(g_udev_device_get_device_file(device), name) == 0)
		device_monitor_handle(action);
}

static void on_prepare_for_sleep(GDBusConnection *connection G_GNUC_UNUSED,
                                  const gchar *sender_name G_GNUC_UNUSED,
                                  const gchar *object_path G_GNUC_UNUSED,
                                  const gchar *interface_name G_GNUC_UNUSED,
                                  const gchar *signal_name G_GNUC_UNUSED,
                                  GVariant *parameters,
                                  gpointer user_data G_GNUC_UNUSED)
{
	gboolean going_to_sleep = FALSE;

	g_variant_get(parameters, "(b)", &going_to_sleep);

	if (going_to_sleep) {
		/* Remember if the port was open so we can reopen it on resume. */
		suspended_while_open = (serial_port_fd != -1);
		if (suspended_while_open)
			interface_close_port();
	} else if (suspended_while_open) {
		/* Built-in UARTs may not emit a udev "add" event on resume.
		 * Try one immediate reconnect; if it fails, keep the flag set so
		 * a later udev event (typical for USB serial) can retry. */
		device_monitor_status(true);
	}
}

void device_monitor_start(void)
{

	const gchar *const subsystems[] = {NULL, NULL};

	udev_client = g_udev_client_new(subsystems);

	/* Monitor device */
	g_signal_connect(G_OBJECT(udev_client), "uevent",
	                 G_CALLBACK(event_udev), NULL);

	/* Subscribe to logind PrepareForSleep to disconnect on suspend and
	 * reconnect on resume. */
	system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
	if (system_bus != NULL) {
		g_dbus_connection_signal_subscribe(system_bus,
		                                   "org.freedesktop.login1",
		                                   "org.freedesktop.login1.Manager",
		                                   "PrepareForSleep",
		                                   "/org/freedesktop/login1",
		                                   NULL,
		                                   G_DBUS_SIGNAL_FLAGS_NONE,
		                                   on_prepare_for_sleep,
		                                   NULL,
		                                   NULL);
	}
}

void device_monitor_stop(void)
{
	if (udev_client != NULL) {
		g_object_unref(udev_client);
		udev_client = NULL;
	}
	if (system_bus != NULL) {
		g_object_unref(system_bus);
		system_bus = NULL;
	}
}

void device_monitor_clear_resume_reconnect(void)
{
	suspended_while_open = FALSE;
}
