# tiny-event-loop
A minimum event loop implemented in c
## Features
1. setTimeout
2. setReadHandler
3. setWriteHandler

## Dependencies
- CMake >= 3.14
- pkg-config
- xxHash (or build with `-DUSE_SIMPLE_HASH=ON` to use a built-in hash instead)

## Build

```bash
git clone --recursive <repo-url>
cd tev
cmake -B build
cmake --build build
```

To install:

```bash
sudo cmake --install build
```

## Test

```bash
./build/test_functional
```
