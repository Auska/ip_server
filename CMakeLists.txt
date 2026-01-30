cmake_minimum_required(VERSION 3.20)
project(ip_local_server VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings (global)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Optimization level
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2 /DNDEBUG /GL)
        add_link_options(/LTCG)
    else()
        add_compile_options(-O3 -DNDEBUG -flto)
        add_link_options(-flto)
    endif()
endif()

# Apply -O3 -DNDEBUG -flto to all C/C++ targets in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT MSVC)
    function(apply_release_lto target)
        target_compile_options(${target} PRIVATE -O3 -DNDEBUG -flto)
    endfunction()
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/external/include
    ${CMAKE_SOURCE_DIR}/external/libmaxminddb-1.12.2/include
    ${CMAKE_SOURCE_DIR}/external/sqlite-autoconf-3510200
    ${CMAKE_SOURCE_DIR}/external/cxxopts-3.3.1/include
)

# Build SQLite3 library first
add_library(sqlite3 STATIC
    ${CMAKE_SOURCE_DIR}/external/sqlite-autoconf-3510200/sqlite3.c
)
target_include_directories(sqlite3 PUBLIC
    ${CMAKE_SOURCE_DIR}/external/sqlite-autoconf-3510200
)

# Disable warnings for SQLite3 (third-party library)
if(NOT MSVC)
    target_compile_options(sqlite3 PRIVATE -Wno-stringop-overread -Wno-array-bounds -Wno-maybe-uninitialized)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(sqlite3 PRIVATE -O3 -DNDEBUG -flto)
    endif()
endif()

# Build maxminddb library second
# Disable maxminddb tests and binaries to only build the library
set(BUILD_TESTING OFF CACHE BOOL "Build maxminddb tests" FORCE)
set(MAXMINDDB_BUILD_BINARIES OFF CACHE BOOL "Build maxminddb binaries" FORCE)
add_subdirectory(external/libmaxminddb-1.12.2)

# Apply compiler options to maxminddb target
if(NOT MSVC)
    target_compile_options(maxminddb PRIVATE -Wno-pedantic -Wno-unused-function)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(maxminddb PRIVATE -O3 -DNDEBUG -flto)
    endif()
endif()

# Build spdlog library third
# Disable spdlog tests, benchmarks, and examples to only build the library
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Build spdlog tests" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "Build spdlog benchmarks" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Build spdlog examples" FORCE)
set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog shared library" FORCE)
add_subdirectory(external/spdlog-1.17.0)

# Apply LTO to spdlog library
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND TARGET spdlog AND NOT MSVC)
    target_compile_options(spdlog PRIVATE -O3 -DNDEBUG -flto)
endif()

# Build cxxopts library
# Disable cxxopts tests and examples to only build the library
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "Build cxxopts tests" FORCE)
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "Build cxxopts examples" FORCE)
add_subdirectory(external/cxxopts-3.3.1)

# Source files
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/database.cpp
    src/mac_database.cpp
    src/http_server.cpp
    src/logger.cpp
    src/xdg.cpp
    src/cache.h
    src/rate_limiter.cpp
    src/auth.cpp
    src/metrics.cpp
    src/password_generator.cpp
)

# Create executable (depends on sqlite3 and maxminddb)
add_executable(ip_server ${SOURCES})

# Enable pedantic warnings for project source code only
if(NOT MSVC)
    target_compile_options(ip_server PRIVATE -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(ip_server PRIVATE -O3 -DNDEBUG -flto)
    endif()
endif()

# Link libraries
target_link_libraries(ip_server
    sqlite3
    maxminddb
    spdlog::spdlog
    cxxopts::cxxopts
    pthread
)

# Strip symbols in Release mode
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(NOT MSVC)
        target_link_options(ip_server PRIVATE -s)
    endif()
endif()

# Set output directory
set_target_properties(ip_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Build tests
option(BUILD_TESTS "Build unit tests" ON)

# Build benchmarks
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)

# Add tests and benchmarks subdirectory
if(BUILD_TESTS OR BUILD_BENCHMARKS)
    add_subdirectory(tests)
endif()