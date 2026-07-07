set_project("ip_local_server")
set_version("2.0.0")

add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

set_languages("c++23")
set_warnings("all", "extra", "pedantic")

add_requires("libmaxminddb", "sqlite3", "spdlog", "nlohmann_json", "cxxopts", "cpp-httplib", "openssl3", "gtest", "benchmark")

option("lto")
    set_default(true)
    set_showmenu(true)
    set_description("Enable Link-Time Optimization (disable for faster dev builds)")

-- Core library: all sources compile once, not once per executable
target("ip_server_core")
    set_kind("static")
    set_targetdir("$(builddir)/lib")
    add_files("src/*.cpp")
    add_files("src/database/*.cpp")
    add_files("src/service/*.cpp")
    remove_files("src/main.cpp")
    add_includedirs("src", "src/database", "src/service")
    add_packages("libmaxminddb", "sqlite3", "spdlog", "nlohmann_json", "cxxopts", "cpp-httplib", "openssl3")
    if is_mode("release") then
        set_optimize("fastest")
        if is_config("lto", true) then
            add_cxflags("-flto")
        end
    end
    set_group("core")

local function exe_settings()
    add_includedirs("src", "src/database", "src/service")
    add_packages("libmaxminddb", "sqlite3", "spdlog", "nlohmann_json", "cxxopts", "cpp-httplib", "openssl3")
    if is_mode("release") then
        set_optimize("fastest")
        if is_config("lto", true) then
            add_cxflags("-flto")
            add_ldflags("-flto")
        end
        add_ldflags("-s")
    end
end

target("ip_server")
    set_kind("binary")
    set_targetdir("$(builddir)/bin")
    add_deps("ip_server_core")
    add_files("src/main.cpp")
    exe_settings()

target("ip_server_tests")
    set_kind("binary")
    set_targetdir("$(builddir)/bin")
    add_deps("ip_server_core")
    add_files("tests/test_*.cpp")
    add_packages("gtest")
    exe_settings()

target("ip_server_benchmarks")
    set_kind("binary")
    set_targetdir("$(builddir)/bin")
    add_deps("ip_server_core")
    add_files("tests/benchmark_*.cpp")
    add_packages("gtest", "benchmark")
    exe_settings()
