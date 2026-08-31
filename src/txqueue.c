/***********************************************************************/
/* txqueue.c                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Non-blocking TX queue for the serial port.                     */
/*      See txqueue.h for the public API.                              */
/*                                                                     */
/***********************************************************************/

#include "config.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "serial.h"
#include "buffer.h"
#include "term_config.h"
#include "config_file.h"
#include "txqueue.h"

/* ---- Internal state ------------------------------------------------ */

/* TX stall timeout: if no byte can be written for this many milliseconds,
 * abort the queue and notify the UI. */
#define TX_QUEUE_CAPACITY   65536
#define TX_STALL_TIMEOUT_MS 5000

static guint8   tx_buf[TX_QUEUE_CAPACITY];
static guint    tx_head         = 0;   /* index of next byte to send */
static gsize    tx_len          = 0;   /* number of bytes pending */
static guint    tx_io_source    = 0;
static guint    tx_stall_source = 0;

/* UI callbacks registered via txqueue_set_callbacks() */
static void (*cb_lock)(void)   = NULL;
static void (*cb_unlock)(void) = NULL;
static void (*cb_stall)(void)  = NULL;

/* File transfer state.  All fields are valid only when tx_file_fp != NULL. */
static FILE    *tx_file_fp               = NULL;
static gboolean tx_file_text_mode        = FALSE;
static goffset  tx_file_total            = 0;
static goffset  tx_file_written          = 0;
static goffset  tx_file_written_base     = 0;     /* bytes in queue before file started */
static gboolean tx_file_paused           = FALSE; /* waiting for char */
static gboolean tx_file_pause_pending    = FALSE; /* LF drained, pause next */
static guint    tx_file_line_delay_src   = 0;     /* GLib timeout for delay */
static void (*cb_file_progress)(goffset written, goffset total, gpointer) = NULL;
static void (*cb_file_done)(gboolean success, gpointer)               = NULL;
static gpointer tx_file_user_data        = NULL;

/* ---- Forward declarations ------------------------------------------ */
static void drain_tx_queue(void);
static void tx_file_complete(gboolean success);
static gssize tx_file_fill_queue(void);
static gboolean tx_drain_cb(GIOChannel *, GIOCondition, gpointer);
static gboolean tx_stall_cb(gpointer);
static gboolean tx_file_delay_cb(gpointer);

/* ---- Public API ----------------------------------------------------- */

void txqueue_set_callbacks(void (*lock_fn)(void),
                           void (*unlock_fn)(void),
                           void (*stall_fn)(void))
{
	cb_lock   = lock_fn;
	cb_unlock = unlock_fn;
	cb_stall  = stall_fn;
}

void txqueue_lock_console(void)
{
	if (cb_lock)
		cb_lock();
}

void txqueue_unlock_console(void)
{
	if (cb_unlock)
		cb_unlock();
}

/* Discard the queue, cancel all pending GLib sources, unlock console. */
void txqueue_abort(void)
{
	if (tx_io_source)
	{
		g_source_remove(tx_io_source);
		tx_io_source = 0;
	}
	if (tx_stall_source)
	{
		g_source_remove(tx_stall_source);
		tx_stall_source = 0;
	}
	if (tx_file_line_delay_src)
	{
		g_source_remove(tx_file_line_delay_src);
		tx_file_line_delay_src = 0;
	}
	tx_head = 0;
	tx_len = 0;
	if (tx_file_fp != NULL)
		tx_file_complete(FALSE);
}

gssize send_serial(const gchar *string, gsize len)
{
	gsize space;
	guint to_queue, tail, chunk1;

	if (len == 0)
		return 0;

	space    = TX_QUEUE_CAPACITY - tx_len;
	to_queue = (guint)MIN(len, space);
	tail     = (guint)((tx_head + tx_len) % TX_QUEUE_CAPACITY);
	chunk1   = MIN(to_queue, TX_QUEUE_CAPACITY - tail);

	memcpy(tx_buf + tail, string, chunk1);
	if (to_queue > chunk1)
		memcpy(tx_buf, string + chunk1, to_queue - chunk1);
	tx_len += to_queue;

	drain_tx_queue();
	return (gssize)to_queue;
}

gboolean txqueue_send_file(const gchar *filename,
                           gboolean text_mode,
                           void (*progress_fn)(goffset, goffset, gpointer),
                           void (*done_fn)(gboolean, gpointer),
                           gpointer user_data)
{
	FILE    *fp;
	goffset  total;

	fp = fopen(filename, "rb");
	if (!fp)
		return FALSE;

	if (fseeko(fp, 0, SEEK_END) != 0 || (total = ftello(fp)) < 0 ||
	    fseeko(fp, 0, SEEK_SET) != 0)
	{
		fclose(fp);
		return FALSE;
	}

	/* Lock the console before setting up file state.  Any keystrokes
	 * already in the queue drain first (they are the remote "accept"
	 * command); the file data follows once tx_buf is empty. */
	txqueue_lock_console();

	tx_file_fp             = fp;
	tx_file_text_mode      = text_mode;
	tx_file_total          = total;
	tx_file_written        = 0;
	tx_file_written_base   = tx_len;  /* account for keystrokes already queued */
	tx_file_paused         = FALSE;
	tx_file_pause_pending  = FALSE;
	cb_file_progress       = progress_fn;
	cb_file_done           = done_fn;
	tx_file_user_data      = user_data;

    drain_tx_queue();

    return TRUE;
}

void txqueue_file_got_char(gchar c)
{
	if (tx_file_fp == NULL || config.car == -1)
		return;
	if ((guchar)c == (guchar)config.car && tx_file_paused)
	{
		tx_file_paused = FALSE;
		drain_tx_queue();
	}
}

/* ---- Internal helpers ---------------------------------------------- */

static void tx_file_complete(gboolean success)
{
	FILE   *fp;
	void   (*done_fn)(gboolean, gpointer);
	gpointer user_data;

	fp        = tx_file_fp;
	done_fn   = cb_file_done;
	user_data = tx_file_user_data;

	tx_file_fp            = NULL;
	tx_file_text_mode     = FALSE;
	tx_file_total         = 0;
	tx_file_written       = 0;
	tx_file_written_base  = 0;
	tx_file_paused        = FALSE;
	tx_file_pause_pending = FALSE;
	cb_file_progress      = NULL;
	cb_file_done          = NULL;
	tx_file_user_data     = NULL;

	fclose(fp);
	txqueue_unlock_console();
	if (done_fn)
		done_fn(success, user_data);
}

static gssize tx_file_fill_queue(void)
{
	gsize nr;

	tx_head = 0;
	tx_len = 0;
	tx_file_pause_pending = FALSE;

	if (!tx_file_text_mode)
	{
		/* Binary mode: bulk read, no line processing. */
		gsize nr = fread(tx_buf, 1, TX_QUEUE_CAPACITY, tx_file_fp);
		if (nr == 0)
			return ferror(tx_file_fp) ? -1 : 0;
		tx_len = nr;
		return (gssize)nr;
	}

	/* Text mode: always read one line; libc handles I/O buffering. */
	if (!fgets((char *)tx_buf, TX_QUEUE_CAPACITY, tx_file_fp))
		return ferror(tx_file_fp) ? -1 : 0;
	nr = strlen((char *)tx_buf);

	/* Apply crlfauto: replace bare \n with \r\n if enabled and there is room. */
	if (term_conf.crlfauto && nr > 0 && tx_buf[nr - 1] == '\n' &&
	    (nr < 2 || tx_buf[nr - 2] != '\r') && nr + 1 < TX_QUEUE_CAPACITY)
	{
		tx_buf[nr] = '\n';
		tx_buf[nr - 1] = '\r';
		nr++;
	}

	tx_len = nr;
	if (nr > 0 && tx_buf[nr - 1] == '\n')
		tx_file_pause_pending = TRUE;
	return (gssize)nr;
}

static gboolean tx_file_delay_cb(gpointer data G_GNUC_UNUSED)
{
	tx_file_line_delay_src = 0;

	if (config.car != -1)
		tx_file_paused = TRUE;  /* delay done; now wait for char */
	else
		drain_tx_queue();

	return G_SOURCE_REMOVE;
}

static gboolean tx_drain_cb(GIOChannel *ch G_GNUC_UNUSED,
                             GIOCondition cond G_GNUC_UNUSED,
                             gpointer data G_GNUC_UNUSED)
{
	drain_tx_queue();

	if (tx_len > 0)
		return G_SOURCE_CONTINUE;

	tx_io_source = 0;

	return G_SOURCE_REMOVE;
}

static gboolean tx_stall_cb(gpointer data G_GNUC_UNUSED)
{
	tx_stall_source = 0;
	txqueue_abort();

    if (cb_stall)
		cb_stall();

    return G_SOURCE_REMOVE;
}

static void drain_tx_queue(void)
{
	if (serial_port_fd == -1)
	{
		txqueue_abort();
		return;
	}

	for (;;)
	{
		/* Drain whatever is in tx_buf. */
		while (tx_len > 0)
		{
			gsize  chunk;
			gssize n;

			/* Send the contiguous segment from tx_head to end-of-buffer.
			 * If data wraps, the next iteration handles the second segment. */
			chunk = MIN(tx_len, TX_QUEUE_CAPACITY - tx_head);
			n     = Send_chars((const gchar *)tx_buf + tx_head, chunk);
			if (n < 0)
			{
				txqueue_abort();
				return;
			}

			if (n == 0)
			{
				/* Kernel buffer full (EAGAIN): arm G_IO_OUT watch. */
				if (tx_io_source == 0)
				{
					GIOChannel *ch = g_io_channel_unix_new(serial_port_fd);
					tx_io_source = g_io_add_watch(ch, G_IO_OUT, tx_drain_cb, NULL);
					g_io_channel_unref(ch);
				}

				if (tx_stall_source)
					g_source_remove(tx_stall_source);

				tx_stall_source = g_timeout_add(TX_STALL_TIMEOUT_MS, tx_stall_cb, NULL);
				return;
			}

			/* Made progress — cancel stall watchdog. */
			if (tx_stall_source)
			{
				g_source_remove(tx_stall_source);
				tx_stall_source = 0;
			}
			if (term_conf.echo)
				put_chars((const gchar *)tx_buf + tx_head, (gsize)n, term_conf.crlfauto);
			if (tx_file_fp != NULL)
			{
				tx_file_written += (goffset)n;
				if (cb_file_progress) {
					goffset reported = (tx_file_written > tx_file_written_base)
					                   ? tx_file_written - tx_file_written_base : 0;
					cb_file_progress(reported, tx_file_total, tx_file_user_data);
				}
			}
			tx_head = (tx_head + (guint)n) % TX_QUEUE_CAPACITY;
			tx_len -= (gsize)n;
		}

		/* tx_buf is now empty. */

		/* After draining a line, apply inter-line pause if configured. */
		if (tx_file_pause_pending)
		{
			tx_file_pause_pending = FALSE;
			if (config.delai > 0)
			{
				/* Delay first; if car is also set, tx_file_delay_cb arms the char wait. */
				tx_file_line_delay_src =
				    g_timeout_add((guint)config.delai, tx_file_delay_cb, NULL);
				return;
			}
			if (config.car != -1)
			{
				/* wait_char mode: pause until txqueue_file_got_char() resumes us. */
				tx_file_paused = TRUE;
				return;
			}
			/* Neither delay nor car: fall through and read next line immediately. */
		}

		/* Nothing more to do if no file is active. */
		if (tx_file_fp == NULL)
			break;

		/* Refill tx_buf from file. In raw mode this is a bulk read.
		 * In script mode it reads exactly one line so we pause only after
		 * that line has actually drained from the queue. */
		{
			gssize nr;

			nr = tx_file_fill_queue();
			if (nr < 0)
			{
				tx_file_complete(FALSE);
				break;
			}
			if (nr == 0)
			{
				/* EOF */
				tx_file_complete(TRUE);
				break;
			}
		}
	}
	/* Queue empty — the active watch (if any) self-disarms on next invocation. */
}
