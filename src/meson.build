sources = [
	'buffer.c',
	'buffer.h',
	'cmdline.c',
	'cmdline.h',
	'device_monitor.c',
	'device_monitor.h',
	'files.c',
	'files.h',
	'gtkterm.c',
	'i18n.c',
	'i18n.h',
	'interface.c',
	'interface.h',
	'logging.c',
	'logging.h',
	'macros.c',
	'macros.h',
	'parsecfg.c',
	'parsecfg.h',
	'serial.c',
	'serial.h',
	'term_config.c',
	'term_config.h',
	'user_signals.c',
	'user_signals.h',
	gresources
]

executable(
	'gtkterm', sources,
	export_dynamic : true,
	dependencies : [
		gtk_deps,
		vte_deps,
        config,
        gudev_deps
	],
	install : true
)
