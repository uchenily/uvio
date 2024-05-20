project('uvio', 'cpp',
  version: '0.0.1',
  default_options: [
    'buildtype=debug',
    'default_library=shared',
    'cpp_std=c++20',
    'warning_level=3'],
  meson_version: '>=0.63.0')

# Use libc++ by default if available instead of libstdc++ when compiling with clang.
cxx = meson.get_compiler('cpp')
if cxx.get_id() == 'clang' and cxx.find_library('c++', required: false).found()
  add_project_arguments(['-stdlib=libc++'], language: 'cpp')
  add_project_link_arguments(['-stdlib=libc++', '-lc++abi'], language: 'cpp')
  message('Compiler: LLVM/clang')
endif

compile_args = []

includes = include_directories('.')

libuv_dep = dependency('libuv')
libbfd_dep = cxx.find_library('bfd', required: false)
dependencies = []
dependencies += libuv_dep

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
