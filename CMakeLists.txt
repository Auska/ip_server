cmake_minimum_required(VERSION 3.20)
project(ip_local_server VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Optimization level
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    if(MSVC)
        add_compile_options(/O2)
    else()
        add_compile_options(-O2)
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/external/include
    ${CMAKE_SOURCE_DIR}/external/libmaxminddb-1.12.2/include
)

# Source files
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/database.cpp
    src/mac_database.cpp
    src/http_server.cpp
    src/logger.cpp
    src/types.cpp
    src/xdg.cpp
    src/cache.h
    src/rate_limiter.cpp
    src/auth.cpp
    src/metrics.cpp
)

# Build maxminddb library
add_subdirectory(external/libmaxminddb-1.12.2)

# Create executable
add_executable(ip_server ${SOURCES})

# Link libraries
target_link_libraries(ip_server
    maxminddb
    sqlite3
    pthread
)

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