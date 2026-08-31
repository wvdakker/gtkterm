project(
	'gtkterm',
	'c',
	version: '2.0.0',
	meson_version : '>= 1.0.0',
	default_options : ['b_lto=true']
)

# Find dependencies
gtk_deps = dependency('gtk4', version : '>= 4.10.0')
vte_deps = dependency('vte-2.91-gtk4', version : '>= 0.70.0')
gudev_deps = dependency('gudev-1.0', version: '>= 230')

# Find install paths
prefix = get_option('prefix')
datadir = prefix / get_option('datadir')
localedir = prefix / get_option('localedir')

# Needed on some platforms for <termios.h>
add_project_arguments('-D_GNU_SOURCE=1',
		      '-D_XOPEN_SOURCE=700',
                      language : ['c'])

# Configure GTKTerm
conf = configuration_data()
release_date = 'April 2026'
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('RELEASE_DATE', release_date)
conf.set_quoted('LOCALEDIR', localedir)

cc = meson.get_compiler('c')
# Preferred path: the C library's baud_t API (glibc >= 2.42, et al.).  The C
# library translates these calls to whatever the running kernel supports -
# including old kernels, via the termios2/BOTHER ioctl internally - so when it
# is present we never have to touch kernel headers ourselves.
have_cfsetobaud = cc.has_function('cfsetobaud',
				  prefix : [ '#define _GNU_SOURCE 1',
					     '#define _XOPEN_SOURCE 700',
					     '#include <termios.h>'])
if have_cfsetobaud
  conf.set('HAVE_CFSETOBAUD', '1')
endif
if cc.has_header('linux/termios.h')
  conf.set('HAVE_LINUX_TERMIOS_H', '1')
endif
# Fallback path, only for a C library that lacks the baud_t API above: talk to
# the kernel termios2/BOTHER ioctl directly for arbitrary baud rates.  Enabled
# only when the real kernel definitions exist, so baud.c never fakes struct
# termios2, TCSETS2 or BOTHER.  Skipped entirely when cfsetobaud is available.
# Probe via <linux/termios.h>, the same header baud.c uses: on powerpc/alpha/
# sparc the TCGETS2/TCSETS2 numbers only resolve when the kernel struct termios2
# is in scope, which <sys/ioctl.h> alone does not guarantee.
if not have_cfsetobaud \
   and cc.has_type('struct termios2', prefix : '#include <linux/termios.h>') \
   and cc.has_header_symbol('linux/termios.h', 'TCSETS2') \
   and cc.has_header_symbol('linux/termios.h', 'TCGETS2') \
   and (cc.has_header_symbol('linux/termios.h', 'BOTHER') \
        or cc.has_header_symbol('linux/termios.h', '__BOTHER'))
  conf.set('HAVE_LINUX_TERMIOS2', '1')
endif
if cc.has_header('sys/sysmacros.h')
  conf.set('HAVE_SYS_SYSMACROS_H', '1')
endif

configure_file(output : 'config.h', configuration : conf)
config = declare_dependency(include_directories : include_directories('.'))

subdir('data')
subdir('src')
subdir('po')

# Debian package target (requires dpkg-buildpackage)
run_target('deb',
	command : [meson.project_source_root() / 'packaging' / 'scripts' / 'build-deb.sh']
)

# Snap package target (requires snapcraft)
run_target('snap',
	command : [meson.project_source_root() / 'packaging' / 'scripts' / 'build-snap.sh']
)

# Flatpak package target (requires flatpak-builder)
run_target('flatpak',
	command : [meson.project_source_root() / 'packaging' / 'scripts' / 'build-flatpak.sh']
)

# AppImage package target (requires linuxdeploy and appimagetool)
run_target('appimage',
	command : [meson.project_source_root() / 'packaging' / 'scripts' / 'build-appimage.sh']
)

# RPM package target (requires rpmbuild)
run_target('rpm',
	command : [meson.project_source_root() / 'packaging' / 'scripts' / 'build-rpm.sh']
)
