set_xmakever("2.9.0")
set_languages("c++20")
set_toolchains("clang")

add_rules("mode.debug", "mode.release")

-- Release optimizations: full LTO + O3
if is_mode("release") then
    set_optimize("fastest")
    set_policy("build.optimization.lto", true)
end

-- On Windows, use xmake package manager for zstd/xxhash
-- On Linux, use system-installed libraries
if is_plat("windows") then
    add_requires("zstd", "xxhash")
end

target("downgrade-patcher")
    set_kind("binary")
    add_rules("qt.widgetapp")
    add_frameworks("QtWidgets", "QtNetwork", "QtConcurrent")
    add_files("src/main.cpp")
    add_files("src/api/*.cpp")
    add_files("src/api/*.h")
    add_files("src/engine/*.cpp")
    add_files("src/engine/*.h")
    add_files("src/ui/*.cpp")
    add_files("src/ui/*.h")
    add_includedirs("src")
    if is_plat("windows") then
        add_packages("zstd", "xxhash")
        add_links("Qt6EntryPoint")
    else
        add_syslinks("zstd", "xxhash")
    end

target("tests")
    set_kind("binary")
    add_rules("qt.console")
    add_frameworks("QtCore", "QtTest", "QtNetwork", "QtConcurrent")
    add_files("tests/test_main.cpp")
    add_files("tests/test_api_client.h")
    add_files("tests/test_hash_cache.h")
    add_files("tests/test_steam_detector.h")
    add_files("tests/test_game_scanner.h")
    add_files("tests/test_patcher.h")
    add_files("src/api/*.cpp")
    add_files("src/api/*.h")
    add_files("src/engine/*.cpp")
    add_files("src/engine/*.h")
    add_includedirs("src")
    if is_plat("windows") then
        add_packages("zstd", "xxhash")
    else
        add_syslinks("zstd", "xxhash")
    end
    set_default(false)
