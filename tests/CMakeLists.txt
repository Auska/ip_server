cmake_minimum_required(VERSION 3.20)

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

# Find Google Test
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Include project headers
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/include
    ${CMAKE_SOURCE_DIR}/external/libmaxminddb-1.12.2/include
)

# Test source files
set(TEST_SOURCES
    test_main.cpp
    test_config.cpp
    test_types.cpp
    test_database.cpp
    test_http_server.cpp
)

# Add source files from src directory
set(PROJECT_SOURCES
    ${CMAKE_SOURCE_DIR}/src/config.cpp
    ${CMAKE_SOURCE_DIR}/src/database.cpp
    ${CMAKE_SOURCE_DIR}/src/http_server.cpp
    ${CMAKE_SOURCE_DIR}/src/types.cpp
    ${CMAKE_SOURCE_DIR}/src/logger.cpp
    ${CMAKE_SOURCE_DIR}/src/xdg.cpp
)

# Create test executable
add_executable(ip_server_tests ${TEST_SOURCES} ${PROJECT_SOURCES})

# Link libraries
target_link_libraries(ip_server_tests
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    pthread
    maxminddb
)

# Enable testing
enable_testing()

# Add test
add_test(NAME ip_server_tests COMMAND ip_server_tests)

# Build benchmarks
if(BUILD_BENCHMARKS)
    # Find Google Benchmark using pkg-config
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(BENCHMARK REQUIRED benchmark)

    include_directories(${BENCHMARK_INCLUDE_DIRS})

    # Benchmark source files
    set(BENCHMARK_SOURCES
        benchmark_database.cpp
    )

    # Create benchmark executable
    add_executable(ip_server_benchmarks ${BENCHMARK_SOURCES} ${PROJECT_SOURCES})

    # Link libraries
    target_link_libraries(ip_server_benchmarks
        ${BENCHMARK_LIBRARIES}
        pthread
        maxminddb
    )

    # Set output directory
    set_target_properties(ip_server_benchmarks PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()