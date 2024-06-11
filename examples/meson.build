all_examples_sources = [
  'echo_server.cpp',
  'channel_server.cpp',
  'channel_client.cpp',
  'http_server.cpp',
  'socks5_server.cpp',
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

websocket_examples_sources = [
  'websocket_server.cpp',
  'websocket_client.cpp',
]
if openssl_dep.found()
  foreach source: websocket_examples_sources
    target_name = source.replace('.cpp', '')
    target = executable(target_name, source,
      include_directories: includes,
      cpp_args: compile_args,
      dependencies: dependencies + openssl_dep)

  test(target_name, target)
  endforeach
endif
