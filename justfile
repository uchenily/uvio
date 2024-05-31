# build (use clang) by default
default: build-all

# show this information
help:
    @just --list

# build with clang
build-all:
    meson compile -C build

# build a specific target
build TARGET:
    meson compile -C build {{TARGET}}

# setup use clang
setup:
    meson setup --reconfigure build --cross-file cross-clang.ini

# build with gcc
build-gcc:
    meson compile -C build-gcc

# setup use gcc
setup-gcc:
    meson setup --reconfigure build-gcc

# setup cmake build env
setup-cmake:
    cmake -B build-cmake -G Ninja

# use cmake to build
build-cmake:
    cmake --build build-cmake

# run a specific test
run TARGET:
    meson test -C build {{TARGET}}

# test a specific test
test TEST:
    meson test -C build {{TEST}} --verbose

# run pre-commit
pre-commit:
    pre-commit run -a

# install(dry-run)
dry-run:
    meson install -C build --no-rebuild --only-changed --dry-run --destdir `pwd`/install

# install library
install:
    meson install -C build --no-rebuild --only-changed --destdir `pwd`/install
