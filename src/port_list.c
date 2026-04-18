#include <glib.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#include "config.h"
#include "port_list.h"

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h>
#endif

/* ------------------------------------------------------------------ */
/* Device path patterns                                                */
/* ------------------------------------------------------------------ */

struct device_path {
	const char    *pat;
	unsigned long  major;
	unsigned long  minor;
	unsigned long  nminors;   /* 0 = no device number matching */
};

static const struct device_path default_device_paths[] =
{
	{ "/dev/ttyS*",           0, 0, 0 },
	{ "/dev/tts/[0-9]*",      0, 0, 0 },
	{ "/dev/usb/tts/[0-9]*",  0, 0, 0 },
	{ NULL, 0, 0, 0 }
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static inline char *skip_word(char *p)
{
	while (*p && !isspace((unsigned char)*p))
		p++;
	return p;
}

static inline char *skip_space(char *p)
{
	while (isspace((unsigned char)*p))
		p++;
	return p;
}

/* ------------------------------------------------------------------ */
/* Enumerate device-path patterns from /proc/tty/drivers (Linux only) */
/* ------------------------------------------------------------------ */

static const struct device_path *get_device_paths(void)
{
#ifdef __linux__
	GArray *array;
	char    line[BUFSIZ];
	FILE   *f = fopen("/proc/tty/drivers", "r");
	if (!f)
		return default_device_paths;

	array = g_array_new(TRUE, FALSE, sizeof(struct device_path));
	while (fgets(line, sizeof line, f))
	{
		struct device_path devp;
		char *path, *p, *pat;
		struct stat st;

		p = line;
		p = skip_word(p);
		if (p == line || !*p)
			continue;

		*p++ = '\0';
		p = skip_space(p);
		if (*p != '/')
			continue;
		path = p;
		p = skip_word(p);
		if (p == path || !*p)
			continue;

		*p++ = '\0';
		devp.major = strtoul(p, &p, 10);
		if (!isspace((unsigned char)*p))
			continue;

		devp.minor = strtoul(p, &p, 10);
		devp.nminors = 1;
		if (*p == '-')
			devp.nminors = strtoul(++p, &p, 10) - devp.minor + 1;
		if (!isspace((unsigned char)*p))
			continue;

		p = skip_space(p);
		if (strncmp(p, "serial", 6))
			continue;

		/* This should be a serial driver */
		pat = "[0-9]*";
		if (!stat(path, &st))
		{
			if (S_ISDIR(st.st_mode))
				pat = "/[0-9]*";
			else if (devp.nminors == 1)
				pat = "";
		}
		devp.pat = g_strdup_printf("%s%s", path, pat);
		array = g_array_append_vals(array, &devp, 1);
	}

	fclose(f);

	if (array->len)
	{
		const struct device_path *dpp = (void *)array->data;
		g_array_free(array, FALSE);
		return dpp;
	}
	g_array_unref(array);
#endif
	return default_device_paths;
}

static void free_device_paths(const struct device_path *paths)
{
	if (paths == default_device_paths)
		return;

	for (const struct device_path *dpp = paths; dpp->pat; dpp++)
		g_free((void *)dpp->pat);

	g_free((void *)paths);
}

/* ------------------------------------------------------------------ */
/* Port sorting                                                        */
/* ------------------------------------------------------------------ */

static int port_priority(const char *path)
{
	if (strstr(path, "ttyACM")) return 0;
	if (strstr(path, "ttyUSB")) return 1;
	return 2;
}

static int compare_seminum(const void *a, const void *b)
{
	const char    *sa = a, *sb = b;
	char          *ea, *eb;
	unsigned long  na, nb;
	unsigned char  ca, cb;
	gboolean       da, db;
	int            pa = port_priority(sa), pb = port_priority(sb);

	if (pa != pb)
		return pa - pb;

	while ((ca = *sa))
	{
		cb = *sb;
		if (!cb) return 1;

		da = isdigit(ca);
		db = isdigit(cb);

		if (da) {
			if (!db) return -1;
			na = strtoul(sa, &ea, 10);
			nb = strtoul(sb, &eb, 10);
			if (na != nb) return na < nb ? -1 : 1;
			if ((ea - sa) != (eb - sb))
				return (ea - sa) < (eb - sb) ? -1 : 1;
			sa = ea; sb = eb;
		} else {
			if (db) return 1;
			if (ca != cb) return (int)ca - (int)cb;
			sa++; sb++;
		}
	}
	return *sb != 0;
}

/* ------------------------------------------------------------------ */
/* Accessibility check                                                 */
/* ------------------------------------------------------------------ */

static int is_serial_port(const char *path, const struct device_path *dp)
{
	struct stat st;

	if (access(path, R_OK | W_OK))
		return FALSE;

	if (stat(path, &st) || !S_ISCHR(st.st_mode))
		return FALSE;

#if defined(major) && defined(minor)
	if (!dp->nminors)
		return TRUE;
	if (dp->major != major(st.st_rdev))
		return FALSE;
	if ((unsigned long)minor(st.st_rdev) - dp->minor >= dp->nminors)
		return FALSE;
#endif
	return TRUE;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

GPtrArray *serial_find_ports(gchar **no_ports_msg)
{
	const struct device_path *device_paths = get_device_paths();
	GPtrArray                *ports        = g_ptr_array_new();

	for (const struct device_path *devp = device_paths; devp->pat; devp++)
	{
		glob_t   gl;
		char   **filep;

		memset(&gl, 0, sizeof gl);
		if (glob(devp->pat, GLOB_NOSORT, NULL, &gl))
			continue;

		for (filep = gl.gl_pathv; *filep; filep++)
		{
			if (is_serial_port(*filep, devp))
				g_ptr_array_add(ports, g_strdup(*filep));
		}
		globfree(&gl);
	}

	g_ptr_array_sort_values(ports, compare_seminum);

	if (!ports->len && no_ports_msg)
	{
		/* Build list of searched patterns for the error message */
		const struct device_path *dp;
		gchar *str, *p, *ep;
		gsize len = 1;

		for (dp = device_paths; dp->pat; dp++)
			len += strlen(dp->pat) + 2;

		str = p = g_malloc(len);
		ep = str + len - 1;
		for (dp = device_paths; dp->pat; dp++)
		{
			*p++ = '\t';
			p += g_strlcpy(p, dp->pat, ep - p);
			*p++ = '\n';
		}
		*p = '\0';

		*no_ports_msg = g_strdup_printf(
		    _("No serial devices found!\n"
		      "\n"
		      "Searched the following device path patterns:\n"
		      "%s\n"
		      "Enter a different device path in the 'Port' box.\n"), str);
		g_free(str);
	}

	free_device_paths(device_paths);
	return ports;
}
