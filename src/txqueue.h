/***********************************************************************/
/* txqueue.h                                                           */
/* ---------                                                           */
/*           GTKTerm Software                                          */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Non-blocking TX queue for the serial port.                     */
/*      Buffers outgoing bytes and drains them via a G_IO_OUT watch    */
/*      when the kernel TTY output buffer is full (EAGAIN).            */
/*      A stall watchdog discards the queue and notifies the UI if     */
/*      nothing can be written for TX_STALL_TIMEOUT_MS milliseconds.  */
/*                                                                     */
/***********************************************************************/

#ifndef TXQUEUE_H_
#define TXQUEUE_H_

#include <glib.h>

/* Register UI callbacks before the first call to send_serial().
 *   lock_fn   — block console input (called by txqueue_lock_console)
 *   unlock_fn — unblock console input (called by txqueue_unlock_console)
 *   stall_fn  — notify user that TX is stuck (called by stall watchdog)
 * All three may be NULL (no-op). */
void txqueue_set_callbacks(void (*lock_fn)(void),
                           void (*unlock_fn)(void),
                           void (*stall_fn)(void));

/* Explicitly lock/unlock console input.  Call from code that sends
 * multi-chunk data (e.g. file transfer) to prevent keystrokes from
 * interleaving with the transfer. */
void txqueue_lock_console(void);
void txqueue_unlock_console(void);

/* Queue bytes for transmission and attempt an immediate drain.
 * Any bytes not written immediately are held in an internal buffer and
 * flushed via a G_IO_OUT watch once the kernel TTY buffer has room.
 * All send paths must go through this function.
 * Returns len on success (bytes accepted into the queue). */
gssize send_serial(const gchar *string, gsize len);

/* Stream an open file descriptor to the serial port.
 * txqueue reads from fd in chunks and feeds the TX queue; the caller
 * must not call send_serial() or close fd while the transfer is active.
 *   fd            — open file descriptor (txqueue will close it when done)
 *   total         — total byte count (for progress reporting)
 *   line_delay_ms — if > 0, pause this many ms after each LF sent
 *   wait_char     — if != -1, pause after each LF until that char is
 *                   received (call txqueue_file_got_char() from the RX path)
 *   progress_fn   — called after each chunk is written (may be NULL)
 *   done_fn       — called on completion or abort (may be NULL)
 *   user_data     — passed to progress_fn and done_fn
 * Console input is locked for the duration. */
/* Queue a file for transmission.
 * text_mode=TRUE: line-by-line with crlfauto, delay and wait_char pacing.
 * text_mode=FALSE: raw binary bulk read, no line processing.
 * Returns FALSE if the file cannot be opened or seeked.
 * progress_fn is called after each chunk; done_fn on completion or abort.
 * Console input is locked for the duration. */
gboolean txqueue_send_file(const gchar *filename,
                           gboolean text_mode,
                           void (*progress_fn)(goffset written, goffset total, gpointer user_data),
                           void (*done_fn)(gboolean success, gpointer user_data),
                           gpointer user_data);

/* Call from the serial receive path with each incoming byte during a
 * file transfer that uses wait_char.  No-op otherwise. */
void txqueue_file_got_char(gchar c);

/* Discard the queue, cancel all pending GLib sources, and unlock console
 * input.  Call on port close, write error, or application shutdown. */
void txqueue_abort(void);

#endif /* TXQUEUE_H_ */
