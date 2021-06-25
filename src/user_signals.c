#include <gtk/gtk.h>
#include "interface.h"

static gboolean handle_usr1(gpointer user_data)
{
	interface_open_port();
	return G_SOURCE_CONTINUE;
}

static gboolean handle_usr2(gpointer user_data)
{
	interface_close_port();
	return G_SOURCE_CONTINUE;
}

void user_signals_catch(void)
{
	g_unix_signal_add(SIGUSR1, (GSourceFunc) handle_usr1, NULL);
	g_unix_signal_add(SIGUSR2, (GSourceFunc) handle_usr2, NULL);
}
