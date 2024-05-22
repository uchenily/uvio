all_examples_sources = [
  'echo_server.cpp',
  'channel_server.cpp',
  'channel_client.cpp',
]

foreach source: all_examples_sources
  target = source.replace('.cpp', '')
  target = executable(target, source,
    include_directories: includes,
    cpp_args: compile_args,
    dependencies: dependencies)
endforeach
