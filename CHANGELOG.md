# Changelog

## v1.3.6
- Switch build system from Makefile to CMake
- Remove bundled xxHash; link to system xxHash via pkg-config
- Add functional test suite with auto checks
- Add GitHub Actions CI pipeline (Linux & macOS)
- Remove version macro from header
- Add API documentation comments to header

## v1.3.5
- Add Makefile install and uninstall targets
- Remove unused `fd` field from `tev_fd_handler_t`
- Rewrite header with `#pragma once`, `extern "C"`, and doxygen comments
- Update submodules

## v1.3.4
- Add shared library (.so) build target with soversion
- Add `-fPIC` to default CFLAGS
- Pass CFLAGS to submodule builds

## v1.3.3
- Fix timeout calculation: use `clock_gettime(CLOCK_MONOTONIC)` instead of `gettimeofday` to avoid issues with wall clock changes

## v1.3.2
- Make it safe to free fd handler inside a read handler callback
- Limit epoll/kqueue batch size to 1 to support safe handler removal

## v1.3.1
- Handle `EPOLLHUP` events by dispatching to the read handler

## v1.3.0
- Add macOS support via kqueue (alongside Linux epoll)
- Remove unused helper functions

## v1.2.0
- Replace cHeap submodule with new heap library using position-tracked API
- Use `heap_new`/`heap_delete` with set_pos/get_pos callbacks for O(log n) removal
- Add version macro in header

## v1.1.1
- Fix clearing the same timeout multiple times by using a handle-to-timer map
- Use opaque timeout handles instead of raw pointers
- Make internal functions static

## v1.1.0
- Add write handler (`tev_set_write_handler`)
- Refactor fd handler to support independent read and write handlers per fd
- Manage epoll events (EPOLLIN/EPOLLOUT) based on active handlers

## v1.0.0
- Initial release
- Event loop with epoll
- Timeout support (`tev_set_timeout`, `tev_clear_timeout`)
- Read handler support (`tev_set_read_handler`)
