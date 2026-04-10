set_xmakever("2.9.0")

set_languages("c++20")
set_toolchains("clang")

target("downgrade-patcher")
    add_rules("qt.widgetapp")
    add_frameworks("QtWidgets", "QtNetwork")
    add_files("src/main.cpp")
    add_syslinks("zstd", "xxhash")

target("tests")
    add_rules("qt.console")
    add_frameworks("QtCore", "QtTest")
    add_files("tests/test_main.cpp")
    add_files("tests/test_main.h")
    set_default(false)
