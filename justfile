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

# run a specific test
test TEST:
    ./build/tests/{{TEST}}

# run pre-commit
pre-commit:
    pre-commit run -a

# install(dry-run)
dry-run:
    meson install -C build --no-rebuild --only-changed --dry-run --destdir `pwd`/install

# install library
install:
    meson install -C build --no-rebuild --only-changed --destdir `pwd`/install
