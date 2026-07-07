set_project("ip_local_server")
set_version("2.0.0")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

set_languages("c++23")
set_warnings("all", "extra", "pedantic")

add_requires("libmaxminddb", "sqlite3", "spdlog", "nlohmann_json", "cxxopts", "cpp-httplib", "openssl3", "gtest", "benchmark")

target("ip_server")
    set_kind("binary")
    set_targetdir("$(builddir)/bin")
    add_files("src/*.cpp")
    add_files("src/database/*.cpp")
    add_files("src/service/*.cpp")
    add_includedirs("src", "src/database", "src/service")
    add_packages("libmaxminddb", "sqlite3", "spdlog", "nlohmann_json", "cxxopts", "cpp-httplib", "openssl3")
    if is_mode("release") then
        set_optimize("fastest")
        add_cxflags("-flto")
        add_ldflags("-flto", "-s")
    end

target("ip_server_tests")
    set_kind("binary")
    set_targetdir("$(builddir)/bin")
    add_files("tests/test_*.cpp")
    add_files("src/*.cpp")
    add_files("src/database/*.cpp")
    add_files("src/service/*.cpp")
    remove_files("src/main.cpp")
    add_includedirs("src", "src/database", "src/service")
    add_packages("libmaxminddb", "sqlite3", "spdlog", "nlohmann_json", "cxxopts", "cpp-httplib", "openssl3", "gtest")
    if is_mode("release") then
        set_optimize("fastest")
        add_cxflags("-flto")
        add_ldflags("-flto", "-s")
    end

target("ip_server_benchmarks")
    set_kind("binary")
    set_targetdir("$(builddir)/bin")
    add_files("tests/benchmark_*.cpp")
    add_files("src/*.cpp")
    add_files("src/database/*.cpp")
    add_files("src/service/*.cpp")
    remove_files("src/main.cpp")
    add_includedirs("src", "src/database", "src/service")
    add_packages("libmaxminddb", "sqlite3", "spdlog", "nlohmann_json", "cxxopts", "cpp-httplib", "openssl3", "gtest", "benchmark")
    if is_mode("release") then
        set_optimize("fastest")
        add_cxflags("-flto")
        add_ldflags("-flto", "-s")
    end
