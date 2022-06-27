gnome = import('gnome')

# Glade UI files
gresources = gnome.compile_resources('resource', 'gresource.xml', source_dir: '.')

# Desktop file
desktop_config = configuration_data()
desktop_config.set('VERSION', meson.project_version())

desktop_file_configured = configure_file(
	input: 'gtkterm.desktop.in',
	output: 'gtkterm.desktop',
	configuration: desktop_config,
	install_dir: join_paths(
		datadir,
		'applications',
	)
)

desktop_file_validate = find_program('desktop-file-validate', required: false)
if desktop_file_validate.found()
	test(
		'desktop file test',
		desktop_file_validate,
		args: [
			desktop_file_configured
		]
	)
endif

schemas_dir = get_option('prefix') / get_option('datadir') / 'glib-2.0' / 'schemas'
settings_schemas = [ 'com.github.jeija.gtkterm.gschema.xml' ]

install_data(settings_schemas, install_dir: schemas_dir)

# Icon
install_data('gtkterm_256x256.png',
	install_dir: join_paths(datadir, 'icons', 'hicolor', '256x256', 'apps'),
	rename : 'gtkterm.png'
)

install_data('gtkterm_128x128.png',
	install_dir: join_paths(datadir, 'icons', 'hicolor', '128x128', 'apps'),
	rename : 'gtkterm.png'
)

install_data('gtkterm_64x64.png',
	install_dir: join_paths(datadir, 'icons', 'hicolor', '64x64', 'apps'),
	rename : 'gtkterm.png'
)

install_data('gtkterm_48x48.png',
	install_dir: join_paths(datadir, 'icons', 'hicolor', '48x48', 'apps'),
	rename : 'gtkterm.png'
)

install_data('gtkterm_48x48.png',
	install_dir: join_paths(datadir, 'icons', 'hicolor', '48x48', 'apps'),
	rename : 'gtkterm.png'
)

# Manpage
install_man('gtkterm.1')
