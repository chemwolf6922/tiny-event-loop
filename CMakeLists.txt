cmake_minimum_required(VERSION 3.14)
project(tev VERSION 1.3.6 LANGUAGES C)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
add_compile_options(-Wall -Wextra -Werror)

option(USE_SIMPLE_HASH "Use simple built-in hash instead of xxHash (forwarded to map)" OFF)

# Submodules
add_subdirectory(heap)
add_subdirectory(map)

# Library
add_library(tev STATIC tev.c)
target_include_directories(tev PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tev PUBLIC heap map)

# Shared library
add_library(tev_shared SHARED tev.c)
set_target_properties(tev_shared PROPERTIES
    OUTPUT_NAME tev
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_include_directories(tev_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tev_shared PUBLIC heap map)

# Test executable (only when built as top-level project)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_executable(test_functional test/test_functional.c)
    target_link_libraries(test_functional PRIVATE tev)
endif()

# Install
include(GNUInstallDirs)

install(TARGETS tev tev_shared
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES tev.h heap/heap.h map/map.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/tev
)
