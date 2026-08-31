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
	const struct device_path *dpp;

	if (paths == default_device_paths)
		return;

	for (dpp = paths; dpp->pat; dpp++)
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
	int            pa = port_priority(sa), pb = port_priority(sb);

	if (pa != pb)
		return pa - pb;

	while (*sa && *sb)
	{
		if (isdigit((unsigned char)*sa) && isdigit((unsigned char)*sb))
		{
			unsigned long na = strtoul(sa, (char **)&sa, 10);
			unsigned long nb = strtoul(sb, (char **)&sb, 10);
			if (na != nb)
				return na < nb ? -1 : 1;
		}
		else
		{
			if (*sa != *sb)
				return (unsigned char)*sa - (unsigned char)*sb;
			sa++; sb++;
		}
	}
	return (unsigned char)*sa - (unsigned char)*sb;
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

GPtrArray *serial_find_ports(void)
{
	const struct device_path *device_paths = get_device_paths();
	const struct device_path *devp;
	GPtrArray                *ports        = g_ptr_array_new();

	for (devp = device_paths; devp->pat; devp++)
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

	/* Remove duplicates (sort guarantees identical paths are adjacent) */
	{
		guint i = 1;
		while (i < ports->len)
		{
			if (g_str_equal(ports->pdata[i - 1], ports->pdata[i]))
			{
				g_free(ports->pdata[i]);
				g_ptr_array_remove_index(ports, i);
			}
			else
				i++;
		}
	}

	free_device_paths(device_paths);
	return ports;
}
