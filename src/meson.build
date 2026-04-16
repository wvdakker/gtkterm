prog_sh = find_program('sh')
baudrates_h = custom_target(
	    'baudrates.h',
	    output : 'baudrates.h',
	    input : 'baudrates.sh',
	    command : [prog_sh, '@INPUT@', '@OUTPUT@'])

sources = [
	'baud.c',
	'baudrates.c',
	baudrates_h,
	'buffer.c',
	'buffer.h',
	'cmdline.c',
	'cmdline.h',
	'config_file.c',
	'config_file.h',
	'config_dialog.c',
	'config_dialog.h',
	'device_monitor.c',
	'device_monitor.h',
	'files.c',
	'files.h',
	'gtkterm.c',
	'interface.c',
	'interface.h',
	'logging.c',
	'logging.h',
	'macros.c',
	'macros.h',
	'port_config_dialog.c',
	'port_config_dialog.h',
	'port_list.c',
	'port_list.h',
	'search.c',
	'search.h',
	'serial.c',
	'serial.h',
	'term_config.c',
	'term_config.h',
	'terminal_config.c',
	'terminal_config.h',
	'user_signals.c',
	'user_signals.h',
	gresources
]

executable(
	'gtkterm', sources,
	export_dynamic : true,
	c_args : [
		'-Wno-unused-parameter',
		'-Wno-missing-field-initializers',
		# Hardening flags
		'-D_FORTIFY_SOURCE=2',
		'-fstack-protector-strong',
		'-fstack-clash-protection',
		'-fcf-protection=full',
		'-Wformat=2',
		'-Wformat-security',
	],
	link_args : [
		'-Wl,-z,relro',
		'-Wl,-z,now',
		'-Wl,-z,noexecstack',
	],
	dependencies : [
		gtk_deps,
		vte_deps,
		config,
		gudev_deps,
		dependency('threads')
	],
	install : true
)
