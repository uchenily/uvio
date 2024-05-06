project('uvio', 'cpp',
  version: '0.0.1',
  default_options: [
    'cpp_std=c++20',
    'warning_level=3'],
  meson_version: '>=1.0.0')


compile_args = []

includes = include_directories('.')

libuv_dep = dependency('libuv')
dependencies = []
dependencies += libuv_dep

subdir('tests')
