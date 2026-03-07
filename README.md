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

## Buildroot Integration

To add this library as a Buildroot package, create the following files in your Buildroot external tree or in `package/tev/`:

**package/tev/Config.in**
```
config BR2_PACKAGE_TEV
	bool "tev"
	depends on BR2_PACKAGE_XXHASH
	help
	  A minimal event loop library implemented in C.

	  https://github.com/chemwolf6922/tiny-event-loop
```

**package/tev/tev.mk**
```makefile
TEV_VERSION = v1.3.6
TEV_SITE = https://github.com/chemwolf6922/tiny-event-loop.git
TEV_SITE_METHOD = git
TEV_GIT_SUBMODULES = YES
TEV_INSTALL_STAGING = YES
TEV_DEPENDENCIES = xxhash
TEV_LICENSE = MIT

$(eval $(cmake-package))
```

Then add `source "package/tev/Config.in"` to your `package/Config.in` and enable `BR2_PACKAGE_TEV` in your defconfig.
