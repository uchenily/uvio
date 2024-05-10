all_gtests_sources = [
  'gtest_main.cpp',
  'gtest_foo.cpp',
  # 'gtest_tcp.cpp',
]

gtest_dep = dependency('gtest')
thread_dep = dependency('threads')
all_gtests_deps = [
  gtest_dep,
  thread_dep,
]

executable('all_gtests',
  all_gtests_sources,
  include_directories: includes,
  dependencies: dependencies + all_gtests_deps)
