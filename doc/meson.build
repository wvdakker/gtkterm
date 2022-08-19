cdata = configuration_data()
cdata.set('TOP_SRCDIR', meson.global_source_root())
cdata.set('TOP_BUILDDIR', meson.global_build_root())
cdata.set('README_PATH', join_paths(meson.global_source_root(),'README_source.md'))
cdata.set('PROJECT_VERSION', meson.project_version())
cdata.set('PROJECT_NAME', meson.project_name())

doxyfile = configure_file(input: 'Doxyfile.in',
                          output: 'Doxyfile',
                          configuration: cdata,
                          install: false)

doxygen = find_program('doxygen')

doc_target = run_target('doc',command: [doxygen, doxyfile])
