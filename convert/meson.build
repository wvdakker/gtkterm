gtkterm_conv_sources = [
	'gtkterm_conv.c',
	'resource_file_conv.c',
	'old_config.c',
	'parsecfg.c',
	'macros.c'
]

executable(
	'gtkterm_conv', 
	[gtkterm_conv_sources],
	export_dynamic : true,
	dependencies : [
		gtk_deps,
		config,
	],
	install : true
)
