all_tests_sources = [
  'test_log.cpp',
  'test_backtrace.cpp',
  'test_libuv.cpp',
  'test_task.cpp',
  'test_timer.cpp',
  'coro_timer.cpp',
  'test_spawn.cpp',
  'test_server.cpp',
  'coro_server.cpp',
  'test_client.cpp',
  'coro_client.cpp',
  'test_work.cpp',
  'coro_work.cpp',
  'test_latch.cpp',
  'test_dns.cpp',
  'test_buffered.cpp',
  'test_buffered2.cpp',
]

foreach source: all_tests_sources
  executable(source.replace('.cpp', ''), source,
    include_directories: includes,
    cpp_args: compile_args,
    dependencies: dependencies)
endforeach

subdir('gtests')
