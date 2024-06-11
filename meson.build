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
# from https://github.com/archibate/debug-hpp/blob/main/msvc_joke.txt
if cpp.get_id() == 'msvc'
  add_project_arguments([
    '/DNOMINMAX',
    '/D_USE_MATH_DEFINES',
    '/utf-8',
    '/bigobj',
    '/Zc:preprocessor',
    '/permissive-',
    '/EHsc'
  ], language: 'cpp')
endif

libuv_dep = dependency('libuv')
libbfd_dep = cpp.find_library('bfd', required: false)
curl_dep = cpp.find_library('curl', required: false)
if curl_dep.found()
  compile_args += '-DHAS_CURL'
endif
openssl_dep = dependency('openssl', required: false)
if openssl_dep.found()
  compile_args += '-DHAS_SSL'
endif

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
