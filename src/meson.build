gtkterm_sources = [
	'gtkterm_cmdline.c',
	'gtkterm.c',
	'gtkterm_window.c',
	'macros.c',
	'gtkterm_configuration.c',
	'gtkterm_serial_port.c',
	'gtkterm_terminal.c',
	'gtkterm_buffer.c',
	gresources
]

executable(
	'gtkterm', 
	[gtkterm_sources],
	export_dynamic : true,
	dependencies : [
		gtk_deps,
		vte_deps,
		config,
		gudev_deps
	],
	install : true
)
