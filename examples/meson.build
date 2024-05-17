all_examples_sources = [
  'echo_server.cpp',
]

foreach source: all_examples_sources
  executable(source.replace('.cpp', ''), source,
    include_directories: includes,
    cpp_args: compile_args,
    dependencies: dependencies)
endforeach
