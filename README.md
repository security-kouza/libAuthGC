# libAuthGC

[![License: LGPL v3](https://img.shields.io/badge/License-LGPL_v3-blue.svg)](https://www.gnu.org/licenses/lgpl-3.0)

A C++ library for authenticated half-gates garbling scheme.

## Prerequisites

- AVX2 compatible CPU(s)
- OpenSSL Library (Tested on version `3.6.0`)
- Basic C/C++ development tools (like compilers, CMake, Git...)
- Currently only working with clang on Linux. Tested with clang `21.1.6`, Linux kernel 6.18.3.

## Usage

You can integrate this library directly using CMake. (eg. `add_subdiretory`, ...)

It's also possible to manually link the library `libAuthGC.a` (refer to [Build](#build)). The header files are under `include/`.

## Build

1. `git submodule update --init --recursive`
2. `cmake -B build && cd build/ && make`

And the `libAuthGC.a` will be located under `build/archive/`

## Test/Benchmark

To build the test program, run `cmake -DENABLE_TEST=ON -B build && cd build/ && make`

The test binary `AuthGC-test` is built under `build/bin` (or other `<path-to-build>/bin`).

## Other Notes

- If macro `DEBUG_FIXED_SEED` is defined, seed `0` is used. 
- Use of `ENABLE_RDSEED` is deprecated, since most Linux distributions already use `RDSEED` and other hardware randomness to seed `/dev/urandom`.

## TODO

We are working on more detailed documentation for this library.
