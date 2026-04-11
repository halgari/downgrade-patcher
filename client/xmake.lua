set_xmakever("2.9.0")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

if is_mode("release") then
    set_optimize("fastest")
    set_policy("build.optimization.lto", true)
end

if is_plat("windows") then
    -- Windows: static build targeting MSVC ABI via clang
    set_toolchains("clang")
    set_runtimes("MT")

    -- zstd and xxhash via xmake package manager (static)
    add_requires("zstd", {configs = {shared = false}})
    add_requires("xxhash", {configs = {shared = false}})
else
    -- Linux: system libraries, dynamic Qt
    set_toolchains("clang")
end

target("downgrade-patcher")
    set_kind("binary")
    add_files("src/main.cpp")
    add_files("src/api/*.cpp")
    add_files("src/api/*.h")
    add_files("src/engine/*.cpp")
    add_files("src/engine/*.h")
    add_files("src/ui/*.cpp")
    add_files("src/ui/*.h")
    add_includedirs("src")
    if is_plat("windows") then
        -- Static Qt: use qt.widgetapp_static rule
        -- Qt path set via: xmake f --qt=C:\Qt6Static
        add_rules("qt.widgetapp_static")
        add_frameworks("QtWidgets", "QtNetwork", "QtConcurrent")
        add_packages("zstd", "xxhash")
    else
        add_rules("qt.widgetapp")
        add_frameworks("QtWidgets", "QtNetwork", "QtConcurrent")
        add_syslinks("zstd", "xxhash")
    end

target("tests")
    set_kind("binary")
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
        add_rules("qt.console_static")
        add_frameworks("QtCore", "QtTest", "QtNetwork", "QtConcurrent")
        add_packages("zstd", "xxhash")
    else
        add_rules("qt.console")
        add_frameworks("QtCore", "QtTest", "QtNetwork", "QtConcurrent")
        add_syslinks("zstd", "xxhash")
    end
    set_default(false)
