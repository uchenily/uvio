project('uvio', 'cpp',
  version: '0.0.1',
  default_options: [
    'buildtype=debug',
    'default_library=shared',
    'cpp_std=c++20',
    'warning_level=3'],
  meson_version: '>=0.63.0')

compile_args = []

includes = include_directories('.')

cpp = meson.get_compiler('cpp')
libuv_dep = dependency('libuv')
libbfd_dep = cpp.find_library('bfd', required: false)
llhttp_dep = dependency('llhttp')

dependencies = []
dependencies += libuv_dep
dependencies += llhttp_dep

if libbfd_dep.found()
  dependencies += libbfd_dep
  add_project_arguments(['-DBACKWARD_HAS_BFD=1'], language: 'cpp')
# add_project_arguments(['-DBACKWARD_HAS_UNWIND=1'], language: 'cpp')
# add_project_arguments(['-DBACKWARD_HAS_LIBUNWIND=1'], language: 'cpp')
# add_project_arguments(['-DBACKWARD_HAS_DW=1'], language: 'cpp') # hang up?
# add_project_link_arguments(['-rdynamic'], language: 'cpp')
endif

subdir('tests')
subdir('examples')
subdir('benchmark')

install_subdir('uvio', install_dir: 'include')
