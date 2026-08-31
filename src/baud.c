/***********************************************************************/
/* baud.c                                                              */
/* -------                                                             */
/*           GTKTerm Software                                          */
/*                      (c) Julien Schmitt                             */
/*                                                                     */
/* ------------------------------------------------------------------- */
/*                                                                     */
/*   Purpose                                                           */
/*      Serial port baud rate setting                                  */
/*      In a separate file as <linux/termios.h> conflicts with	       */
/*	<termios.h>. A struct termios is not valid across a call to    */
/*      set_port_baudrate().                                           */
/*                                                                     */
/*      Three implementations are selected at configure time, from     */
/*      most to least capable:                                         */
/*        1. HAVE_CFSETOBAUD     - modern glibc baud_t API (cfsetobaud)*/
/*        2. HAVE_LINUX_TERMIOS2 - kernel termios2/BOTHER ioctl        */
/*        3. (fallback)          - generic POSIX cfsetospeed           */
/*      Each path is self-contained; none redefines the kernel's       */
/*      structures or macros.                                          */
/*                                                                     */
/*      Returns 0 on error, otherwise the updated baud rate if the     */
/*      kernel reports it.                                             */
/*                                                                     */
/***********************************************************************/

#include <errno.h>

#include <config.h>

#define CHK(x) do { if (x) return 0; } while (0)

#ifdef HAVE_CFSETOBAUD

#include "serial.h"

unsigned int set_port_baudrate(unsigned int baud, int port_fd)
{
	struct termios tio;

	CHK(tcgetattr(port_fd, &tio));
	CHK(cfsetobaud(&tio, baud));
	CHK(cfsetibaud(&tio, baud));
	CHK(tcsetattr(port_fd, TCSANOW, &tio));
	CHK(tcgetattr(port_fd, &tio));
	return cfgetobaud(&tio);
}

#elif defined(HAVE_LINUX_TERMIOS2)

/* <linux/termios.h> supplies struct termios2, BOTHER, CBAUD, CIBAUD and the
 * TCGETS2/TCSETS2 ioctl numbers.  It must come before <sys/ioctl.h>: on the
 * arches where <asm/ioctls.h> encodes these ioctls as _IOR/_IOW of
 * sizeof(struct termios2) (powerpc, alpha, sparc), the kernel struct has to be
 * in scope first or the numbers are computed against the wrong type.
 * <linux/termios.h> pulls in <asm/termbits.h> ahead of <asm/ioctls.h>, so it
 * gets this right; <sys/ioctl.h> is included only for the ioctl() prototype.
 * glibc's <termios.h> must not be included here; serial.h honours NO_TERMIOS.
 *
 * This branch is only compiled when meson confirmed that struct termios2,
 * TCGETS2/TCSETS2 and BOTHER (or __BOTHER) genuinely exist, so we never fake
 * the kernel structures or redefine their macros. */
#include <linux/termios.h>
#include <sys/ioctl.h>

#define NO_TERMIOS
#include "serial.h"

#ifndef CIBAUD
# define CIBAUD (CBAUD << 16)	/* Missing input-speed field on some arches */
#endif
#ifndef IBSHIFT
# define IBSHIFT 16		/* CIBAUD = CBAUD << IBSHIFT (input-speed offset) */
#endif

/* A few C libraries expose the "use c_ispeed/c_ospeed" flag as __BOTHER
 * rather than BOTHER.  Resolve it once into a constant instead of redefining
 * the macro, so an existing BOTHER definition is never clobbered. */
#if defined(BOTHER)
static const tcflag_t OTHER_BAUD = BOTHER;
#else
static const tcflag_t OTHER_BAUD = __BOTHER;
#endif

unsigned int set_port_baudrate(unsigned int baud, int port_fd)
{
	struct termios2 tio;
	speed_t std = find_standard_baudrate(baud);

	CHK(ioctl(port_fd, TCGETS2, &tio));

	tio.c_cflag &= ~(CBAUD | CIBAUD);
	if (std != B0)
		/* Standard rate: use the matching Bxxxx selector, understood by
		 * every driver.  BOTHER is reserved for non-standard rates that
		 * cannot be expressed as a Bxxxx constant. */
		tio.c_cflag |= std | ((tcflag_t)std << IBSHIFT);
	else
		tio.c_cflag |= OTHER_BAUD;
	tio.c_ispeed = tio.c_ospeed = baud;

	CHK(ioctl(port_fd, TCSETS2, &tio));
	CHK(ioctl(port_fd, TCGETS2, &tio));

	/* Report the actual speed the kernel applied; the caller validates it
	 * against the requested rate.  Do NOT additionally require CBAUD==BOTHER:
	 * for a standard rate (e.g. 115200) many drivers normalise CBAUD back to
	 * the matching Bxxxx selector, which is still success. */
	return tio.c_ospeed;
}

#else  /* No baud_t interface, not Linux */

#include "serial.h"

unsigned int set_port_baudrate(unsigned int baud, int port_fd)
{
	struct termios tio;
	speed_t speed;

	if (speed_t_is_sane)
	{
		speed = baud;

		/* Check for type conversion errors */
		if (speed != baud)
			speed = B0;
	} else {
		speed = find_standard_baudrate(baud);
	}

	if (speed == B0)
	{
		errno = EINVAL;
		return 0;
	}

	CHK(tcgetattr(port_fd, &tio));
	CHK(cfsetospeed(&tio, speed));
	CHK(cfsetispeed(&tio, speed));
	CHK(tcsetattr(port_fd, TCSANOW, &tio));
	CHK(tcgetattr(port_fd, &tio));
	return speed_t_to_baud(cfgetospeed(&tio));
}

#endif
