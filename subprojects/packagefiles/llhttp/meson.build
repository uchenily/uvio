project('llhttp', 'c', version: '9.2.1', license: 'MIT')

llhttp_include = include_directories('include')
c_args = []
llhttp_deps = []

config_data = configuration_data()
config_data.set('CMAKE_INSTALL_PREFIX', get_option('prefix'))
config_data.set('CMAKE_INSTALL_LIBDIR', get_option('libdir'))
config_data.set('CMAKE_INSTALL_INCLUDEDIR', get_option('includedir'))
config_data.set('PROJECT_VERSION', meson.project_version())

configure_file(
  input: 'libllhttp.pc.in',
  output: 'libllhttp.pc',
  configuration: config_data,
  install_dir: get_option('libdir') / 'pkgconfig',
  install: true,
)

llhttp_src = files(
  'src/api.c',
  'src/http.c',
  'src/llhttp.c',
)

llhttp_lib = library('llhttp', llhttp_src,
  include_directories: llhttp_include,
  dependencies: llhttp_deps,
  c_args: c_args,
  install: true,
)

llhttp_dep = declare_dependency(
  include_directories: llhttp_include,
  link_with: llhttp_lib,
  dependencies: llhttp_deps,
)

install_headers('include/llhttp.h')
