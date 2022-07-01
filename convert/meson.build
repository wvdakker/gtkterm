gtkterm_conv_sources = [
	'gtkterm_conv.c',
	'../src/resource_file.c',
	'../src/i18n.c',
	'old_config.c',
	'parsecfg.c',
	'../src/macros.c'
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
