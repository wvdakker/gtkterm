/***********************************************************************/
/* device_mintor.h                                                     */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Monitor device to autoreconnect                                */
/*   Written by Kevin Picot - picotk27@gmail.com                       */
/*                                                                     */
/***********************************************************************/

#include <device_monitor.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <libudev.h>
#include <locale.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <serial.h>
#include <interface.h>
#include <glib/gprintf.h>
#include <term_config.h>

#include "interface.h"

extern struct configuration_port config;

static inline void device_monitor_status(const bool connected) {

    gchar *message;

    if (connected)
        interface_open_port();
    else
        interface_close_port();
}

static inline void device_monitor_handle(struct udev_device *dev) {

    if( strcmp(udev_device_get_action(dev), "remove") == 0 ) {
        device_monitor_status(false);
    } else if( strcmp(udev_device_get_action(dev), "add") == 0 ) {
        device_monitor_status(true);
    }
}

static gpointer device_monitor_thread(gpointer data) {

    struct udev *udev;
    struct udev_monitor *mon;

    udev = udev_new();
    mon = udev_monitor_new_from_netlink(udev, "udev");

    if( mon == NULL ) {
        g_printf("%s:%u udev error\n", __func__, __LINE__);
        return NULL;
    }

    udev_monitor_enable_receiving(mon);

    while (1) {

        struct udev_device *dev;

        dev = udev_monitor_receive_device(mon);

        if (dev) {

            const char *n = udev_device_get_devnode(dev);
            const gchar *name = config.port;

            if( n != NULL )
                if( strcmp(n, name) == 0 )
                    device_monitor_handle(dev);

            udev_device_unref(dev);
        }
        usleep(250*1000);
    }
}

extern void device_monitor_start(void) {

    /* Create gtk thread */
    g_thread_new("dev_mon_th", device_monitor_thread, NULL);
}
