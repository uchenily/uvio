all_tests_sources = [
  'test_log.cpp',
  'test_libuv.cpp',
  'test_task.cpp',
  'test_timer.cpp',
  'coro_timer.cpp',
]

foreach source: all_tests_sources
  executable(source.replace('.cpp', ''), source,
    include_directories: includes,
    cpp_args: compile_args,
    dependencies: dependencies)
endforeach

subdir('gtests')
