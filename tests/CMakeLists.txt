cmake_minimum_required(VERSION 3.20)

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

# Link OpenSSL for httplib
find_package(OpenSSL REQUIRED)
target_link_libraries(ip_server_tests OpenSSL::SSL OpenSSL::Crypto)

# Enable testing
enable_testing()

# Add test
add_test(NAME ip_server_tests COMMAND ip_server_tests)