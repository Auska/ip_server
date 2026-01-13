-- xmake.lua for IP Geolocation & AS Lookup Service
-- Build configuration for C++23 project

set_version("1.0.0")
set_config("generate_compile_commands", true)

-- Package dependencies (installed via xrepo)
add_requires("nlohmann_json v3.12.0", "sqlite3 3.51.0+0", "spdlog v1.16.0", 
             "cxxopts v3.3.1", "cpp-httplib v0.28.0", 
             "libmaxminddb 1.12.2", {optional = false})

-- Common compile flags
add_cxxflags("-std=c++23", "-Wall", "-Wextra", "-Wpedantic", "-static", "-s")
add_ldflags("-static", {force = true})
add_includedirs("src")

-- Target: main executable
target("ip_server")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("nlohmann_json", "spdlog", "sqlite3", "cxxopts", 
                 "cpp-httplib", "libmaxminddb")
    add_syslinks("pthread")
    set_targetdir("build/bin")

add_rules("mode.debug", "mode.release")
