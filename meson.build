project(
	'GTKTerm',
	'c',
	version: '1.99.0',
	meson_version : '>= 0.49.0'
)

# Find dependencies
gtk_deps = dependency('gtk4', version : '>= 4.6.0')
vte_deps = dependency('vte-2.91-gtk4', version : '>= 0.69.92')
gudev_deps = dependency('gudev-1.0', version: '>= 230')

# Find install paths
prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))
localedir = join_paths(prefix, get_option('localedir'))

# Configure GTKTerm
conf = configuration_data()
conf.set_quoted('PACKAGE_VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('RELEASE_DATE', 'December 2020')
conf.set_quoted('LOCALEDIR', localedir)

cc = meson.get_compiler('c')
have_serial_h = cc.has_header('linux/serial.h')
if have_serial_h
  conf.set('HAVE_LINUX_SERIAL_H', '1')
endif

doxygen = find_program('doxygen', required : false)
if doxygen.found()
  message('Doxygen found')
else
  warning('Documentation disabled without doxygen')
endif

configure_file(output : 'config.h', configuration : conf)
config = declare_dependency(include_directories : include_directories('.', 'src', '../src'))

subdir('data')
subdir('src')
subdir('convert')
subdir('po')
subdir('doc')

install_man('data/gtkterm.1')

meson.add_install_script('meson_post_install.py')
