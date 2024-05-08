project('uvio', 'cpp',
  version: '0.0.1',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3'],
  meson_version: '>=1.0.0')

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
dependencies = []
dependencies += libuv_dep

subdir('tests')

install_subdir('uvio', install_dir: 'include')
