project('backward-cpp', 'cpp', version: '1.6')

cpp = meson.get_compiler('cpp')

bfd_dep = cpp.find_library('bfd', required: false)
libdw_dep = dependency('libdw', required: false)
# libdwarf_dep = dependency('libdwarf', required: false)
libunwind_dep = dependency('libunwind', required: false)

backward_deps = []

if bfd_dep.found()
  backward_deps += bfd_dep
  backward_deps += dependency('dl')
endif

if libdw_dep.found()
  backward_deps += libdw_dep
endif

# if libdwarf_dep.found()
#   backward_deps += libdwarf_dep
# endif

if libunwind_dep.found()
  backward_deps += libunwind_dep
endif

# install_headers('backward.hpp', subdir: 'backward')

# backward_lib = library('backward-cpp', 'backward.cpp', dependencies: backward_deps, install: true)

backward_dep = declare_dependency(
  include_directories: include_directories('.'),
  dependencies: backward_deps,
  # link_with: backward_lib,
)
