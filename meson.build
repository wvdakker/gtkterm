project(
	'gtkterm',
	'c',
	version: '1.2.1',
	meson_version : '>= 0.46.0'
)

# Find dependencies
gtk_deps = dependency('gtk+-3.0', version : '>= 3.0.0')
vte_deps = dependency('vte-2.91', version : '>= 0.28.0')
gudev_deps = dependency('gudev-1.0', version: '>= 230')

# Find install paths
prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
localedir = join_paths(prefix, get_option('localedir'))

# Configure GTKTerm
conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('RELEASE_DATE', 'July 2022')
conf.set_quoted('LOCALEDIR', localedir)

cc = meson.get_compiler('c')
have_serial_h = cc.has_header('linux/serial.h')
if have_serial_h
  conf.set('HAVE_LINUX_SERIAL_H', '1')
endif

configure_file(output : 'config.h', configuration : conf)
config = declare_dependency(include_directories : include_directories('.'))

subdir('data')
subdir('src')
subdir('po')
