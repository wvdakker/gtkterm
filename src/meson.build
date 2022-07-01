gtkterm_sources = [
	'buffer.c',
	'cmdline.c',
	'files.c',
	'gtkterm.c',
	'i18n.c',
	'interface.c',
	'macros.c',
	'serial.c',
	'term_config.c',
	'resource_file.c',
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
