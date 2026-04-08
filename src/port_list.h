#ifndef PORT_LIST_H_
#define PORT_LIST_H_

#include <glib.h>

/*
 * Scan the system for accessible serial ports.
 *
 * Returns a GPtrArray of owned gchar* paths, sorted by priority
 * (ttyACM > ttyUSB > ttyS…) and then numerically.
 * The caller is responsible for freeing each element and the array:
 *   for (guint i = 0; i < ports->len; i++) g_free(ports->pdata[i]);
 *   g_ptr_array_free(ports, TRUE);
 *
 * If no ports are found and no_ports_msg is non-NULL it is set to an
 * allocated human-readable message string listing the patterns that
 * were searched (caller must g_free it).
 */
GPtrArray *serial_find_ports(gchar **no_ports_msg);

#endif /* PORT_LIST_H_ */
