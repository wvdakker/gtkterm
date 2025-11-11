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
#include <interface.h>
#include <term_config.h>
#include <gudev/gudev.h>

#include "serial.h"
#include "interface.h"

extern struct configuration_port config;

static inline void device_monitor_status(const bool connected)
{
	if (connected) {
		if (config.autoreconnect_enabled)
			interface_open_port();
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

extern void device_monitor_start(void)
{

	const gchar *const subsystems[] = {NULL, NULL};

	/* Initial check */
	GUdevClient *udev_client = g_udev_client_new(subsystems);

	if (g_udev_client_query_by_device_file(udev_client, config.port) == NULL) {
		device_monitor_status(false);
	} else {
		device_monitor_status(true);
	}

	/* Monitor device */
	g_signal_connect(G_OBJECT(udev_client), "uevent",
	                 G_CALLBACK(event_udev), NULL);
}
