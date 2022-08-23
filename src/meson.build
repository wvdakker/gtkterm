gtkterm_sources = [
	'cmdline.c',
	'gtkterm.c',
	'gtkterm_messages.c',
	'gtkterm_window.c',
	'macros.c',
	'resource_file.c',
	'serial.c',	
	'terminal.c',
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
