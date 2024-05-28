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

curl_examples_sources = [
  'example_curl.cpp',
]
# curl_dep = dependency('libcurl')
curl_dep = cpp.find_library('curl', required: false)
if curl_dep.found()
  foreach source: curl_examples_sources
    target = source.replace('.cpp', '')
    target = executable(target, source,
      include_directories: includes,
      cpp_args: compile_args,
      dependencies: dependencies + curl_dep)
  endforeach
endif
