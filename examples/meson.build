all_examples_sources = [
  'echo_server.cpp',
  'channel_server.cpp',
  'channel_client.cpp',
  'http_server.cpp',
]

foreach source: all_examples_sources
  target_name = source.replace('.cpp', '')
  target = executable(target_name, source,
    include_directories: includes,
    cpp_args: compile_args,
    dependencies: dependencies)

  test(target_name, target)
endforeach

curl_examples_sources = [
  'example_curl.cpp',
  'coro_curl.cpp',
]
# curl_dep = dependency('libcurl')
curl_dep = cpp.find_library('curl', required: false)
if curl_dep.found()
  foreach source: curl_examples_sources
    target_name = source.replace('.cpp', '')
    target = executable(target_name, source,
      include_directories: includes,
      cpp_args: compile_args,
      dependencies: dependencies + curl_dep)

  test(target_name, target)
  endforeach
endif
