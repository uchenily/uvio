cpp_benchmarks_sources = [
  'benchmark_uvio.cpp',
]

foreach source: cpp_benchmarks_sources
  executable(source.replace('.cpp', ''), source,
    include_directories: includes,
    cpp_args: compile_args,
    dependencies: dependencies)
endforeach
