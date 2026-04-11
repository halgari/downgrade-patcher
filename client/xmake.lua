set_xmakever("2.9.0")
set_languages("c++20")

add_rules("mode.debug", "mode.release")

if is_mode("release") then
    set_optimize("fastest")
    set_policy("build.optimization.lto", true)
end

if is_plat("windows") then
    -- Windows: use MSVC for static Qt compatibility
    set_toolchains("msvc")
    set_runtimes("MT")
    add_requires("zstd", {configs = {shared = false}})
    add_requires("xxhash", {configs = {shared = false}})
else
    set_toolchains("clang")
end

target("downgrade-patcher")
    set_kind("binary")
    add_files("src/main.cpp")
    add_files("resources/resources.qrc")
    add_files("src/api/*.cpp")
    add_files("src/api/*.h")
    add_files("src/engine/*.cpp")
    add_files("src/engine/*.h")
    add_files("src/ui/*.cpp")
    add_files("src/ui/*.h")
    add_includedirs("src")
    if is_plat("windows") then
        add_rules("qt.widgetapp_static")
        add_frameworks("QtWidgets", "QtNetwork", "QtConcurrent")
        add_packages("zstd", "xxhash")
        -- Static Qt TLS plugin — link the plugin lib + its init object + deps
        add_linkdirs("$(env QT_DIR)/plugins/tls")
        add_linkdirs("$(env QT_DIR)/lib")
        add_links("qschannelbackend")
        add_ldflags(
            "$(env QT_DIR)/plugins/tls/objects-Release/QSchannelBackendPlugin_init/QSchannelBackendPlugin_init.cpp.obj",
            {force = true}
        )
        add_syslinks("secur32", "ncrypt", "crypt32")
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
        add_rules("qt.widgetapp_static")
        add_frameworks("QtCore", "QtTest", "QtNetwork", "QtConcurrent", "QtWidgets")
        add_packages("zstd", "xxhash")
    else
        add_rules("qt.console")
        add_frameworks("QtCore", "QtTest", "QtNetwork", "QtConcurrent")
        add_syslinks("zstd", "xxhash")
    end
    set_default(false)
