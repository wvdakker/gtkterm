/***********************************************************************/
/* device_mintor.h													 */
/* ---------														   */
/*		   GTKTerm Software										  */
/*					  (c) Julien Schmitt							 */
/*																	 */
/* ------------------------------------------------------------------- */
/*																	 */
/*   Purpose														   */
/*	  Monitor device to autoreconnect								*/
/*   Written by Kevin Picot - picotk27@gmail.com					   */
/*																	 */
/***********************************************************************/

#include <device_monitor.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <locale.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <gio/gio.h>
#include <interface.h>
#include <term_config.h>
#include <gudev/gudev.h>

#include "serial.h"
#include "interface.h"

extern struct configuration_port config;

/* True when the port was closed because the system is going to sleep,
 * so we know to reopen it on resume regardless of autoreconnect_enabled. */
static gboolean suspended_while_open = FALSE;

static GUdevClient *udev_client = NULL;

static inline void device_monitor_status(const bool connected)
{
	if (connected) {
		if (config.autoreconnect_enabled || suspended_while_open) {
			suspended_while_open = FALSE;
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

void event_udev(GUdevClient *client, const gchar *action, GUdevDevice *device)
{

	if (!device || !action)
		return;

	if (!g_udev_device_get_device_file(device))
		return;

	const gchar *name = config.port;

	if (strcmp(g_udev_device_get_device_file(device), name) == 0)
		device_monitor_handle(action);
}

static void on_prepare_for_sleep(GDBusConnection *connection,
                                  const gchar *sender_name,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *signal_name,
                                  GVariant *parameters,
                                  gpointer user_data)
{
	gboolean going_to_sleep = FALSE;

	g_variant_get(parameters, "(b)", &going_to_sleep);

	if (going_to_sleep) {
		/* Remember if the port was open so we can reopen it on resume. */
		suspended_while_open = (serial_port_fd != -1);
		if (suspended_while_open)
			interface_close_port();
	} else {
		/* On resume, reconnect immediately only if the device is already
		 * present (built-in UARTs). For USB serial the device hasn't
		 * re-enumerated yet; the udev "add" event will fire later and
		 * device_monitor_status() will reconnect because suspended_while_open
		 * is still set. */
		if (suspended_while_open && udev_client != NULL &&
		    g_udev_client_query_by_device_file(udev_client, config.port) != NULL)
			device_monitor_status(true);
	}
}

extern void device_monitor_start(void)
{

	const gchar *const subsystems[] = {NULL, NULL};

	/* Initial check */
	udev_client = g_udev_client_new(subsystems);

	if (g_udev_client_query_by_device_file(udev_client, config.port) == NULL) {
		device_monitor_status(false);
	} else {
		device_monitor_status(true);
	}

	/* Monitor device */
	g_signal_connect(G_OBJECT(udev_client), "uevent",
	                 G_CALLBACK(event_udev), NULL);

	/* Subscribe to logind PrepareForSleep to disconnect on suspend and
	 * reconnect on resume. */
	GDBusConnection *system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
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
