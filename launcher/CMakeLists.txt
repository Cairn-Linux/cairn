# SPDX-License-Identifier: Apache-2.0
find_package(Qt6 REQUIRED COMPONENTS LinguistTools WaylandClient)
find_package(PkgConfig REQUIRED)
pkg_get_variable(WLR_PROTOCOLS_DIR wlr-protocols pkgdatadir)
if(NOT WLR_PROTOCOLS_DIR)
    message(FATAL_ERROR "The launcher needs wlr-protocols-devel: dnf install wlr-protocols-devel")
endif()

# The client side of wlr-foreign-toplevel-management-unstable-v1, generated
# from the protocol XML that wlr-protocols-devel installs. It reports whether a
# window is hidden and which window it belongs to (ADR-0026). Only the window
# sources use it.
qt_add_library(cairn-foreign-toplevel STATIC
    src/ForeignToplevel.h src/ForeignToplevel.cpp
    src/ForeignToplevelList.h src/ForeignToplevelList.cpp
)
qt_generate_wayland_protocol_client_sources(cairn-foreign-toplevel
    FILES "${WLR_PROTOCOLS_DIR}/unstable/wlr-foreign-toplevel-management-unstable-v1.xml"
)
target_include_directories(cairn-foreign-toplevel PUBLIC src "${CMAKE_CURRENT_BINARY_DIR}")
# PRIVATE on purpose: the QML module targets that link this library would
# otherwise ask for a WaylandClient metatypes file Fedora does not ship.
target_link_libraries(cairn-foreign-toplevel PRIVATE Qt6::WaylandClient)

set(launcher_model_sources
    src/TileModel.h src/TileModel.cpp
    src/Manifest.h src/Manifest.cpp
    src/AppLauncher.h src/AppLauncher.cpp
    src/AppScope.h src/AppScope.cpp
    src/LaunchWindows.h src/LaunchWindows.cpp
    src/GiveUpWatch.h src/GiveUpWatch.cpp
    src/SteamAtLogin.h src/SteamAtLogin.cpp
    src/GridScroller.h src/GridScroller.cpp
    src/CloseRequest.h src/CloseRequest.cpp
    src/LogOut.h src/LogOut.cpp
)
# Needs a Wayland session to do anything, so it stays out of the model tests.
set(launcher_window_sources src/ForeignWindowList.h src/ForeignWindowList.cpp)
# What the launcher tells Footpath: the tiles are its doors.
set(launcher_terminal_sources src/DoorsFromTiles.h src/DoorsFromTiles.cpp)
set(launcher_qml_files qml/Main.qml qml/Tile.qml qml/GrownUpScreen.qml qml/LogOutScreen.qml
    qml/StartingScreen.qml)
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/Tile.qml PROPERTIES QT_RESOURCE_ALIAS Tile.qml)
set_source_files_properties(qml/GrownUpScreen.qml PROPERTIES QT_RESOURCE_ALIAS GrownUpScreen.qml)
set_source_files_properties(qml/LogOutScreen.qml PROPERTIES QT_RESOURCE_ALIAS LogOutScreen.qml)
set_source_files_properties(qml/StartingScreen.qml PROPERTIES QT_RESOURCE_ALIAS StartingScreen.qml)

qt_add_executable(cairn-launcher src/main.cpp)
qt_add_qml_module(cairn-launcher
    URI Cairn.Launcher
    QML_FILES ${launcher_qml_files}
    SOURCES ${launcher_model_sources} ${launcher_window_sources} ${launcher_terminal_sources}
)
target_include_directories(cairn-launcher PRIVATE src)
target_link_libraries(cairn-launcher PRIVATE Qt6::Quick CairnBrand cairn-foreign-toplevel footpath footpathplugin)

qt_add_lupdate(
    SOURCE_TARGETS cairn-launcher
    TS_FILES i18n/launcher_en.ts
    LUPDATE_TARGET launcher_lupdate
)

qt_add_executable(tst_tilemodel tests/tst_tilemodel.cpp ${launcher_model_sources})
target_include_directories(tst_tilemodel PRIVATE src)
target_link_libraries(tst_tilemodel PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME tilemodel COMMAND tst_tilemodel)

qt_add_executable(tst_manifest tests/tst_manifest.cpp ${launcher_model_sources})
target_include_directories(tst_manifest PRIVATE src)
target_link_libraries(tst_manifest PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME manifest COMMAND tst_manifest)

qt_add_executable(tst_applauncher tests/tst_applauncher.cpp ${launcher_model_sources})
target_include_directories(tst_applauncher PRIVATE src)
target_link_libraries(tst_applauncher PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME applauncher COMMAND tst_applauncher)

qt_add_executable(tst_launchwindows tests/tst_launchwindows.cpp src/LaunchWindows.h src/LaunchWindows.cpp)
target_include_directories(tst_launchwindows PRIVATE src)
target_link_libraries(tst_launchwindows PRIVATE Qt6::Core Qt6::Test)
add_test(NAME launchwindows COMMAND tst_launchwindows)

qt_add_executable(tst_giveupwatch tests/tst_giveupwatch.cpp ${launcher_model_sources})
target_include_directories(tst_giveupwatch PRIVATE src)
target_link_libraries(tst_giveupwatch PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME giveupwatch COMMAND tst_giveupwatch)

qt_add_executable(tst_steamatlogin tests/tst_steamatlogin.cpp ${launcher_model_sources})
target_include_directories(tst_steamatlogin PRIVATE src)
target_link_libraries(tst_steamatlogin PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME steamatlogin COMMAND tst_steamatlogin)

qt_add_executable(tst_appscope tests/tst_appscope.cpp src/AppScope.h src/AppScope.cpp)
target_include_directories(tst_appscope PRIVATE src)
target_link_libraries(tst_appscope PRIVATE Qt6::Core Qt6::Test)
add_test(NAME appscope COMMAND tst_appscope)

qt_add_executable(tst_gridscroller tests/tst_gridscroller.cpp ${launcher_model_sources})
target_include_directories(tst_gridscroller PRIVATE src)
target_link_libraries(tst_gridscroller PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME gridscroller COMMAND tst_gridscroller)

qt_add_executable(tst_closerequest tests/tst_closerequest.cpp ${launcher_model_sources})
target_include_directories(tst_closerequest PRIVATE src)
target_link_libraries(tst_closerequest PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME closerequest COMMAND tst_closerequest)

qt_add_executable(tst_logout tests/tst_logout.cpp ${launcher_model_sources})
target_include_directories(tst_logout PRIVATE src)
target_link_libraries(tst_logout PRIVATE Qt6::Qml Qt6::Test)
target_compile_definitions(tst_logout PRIVATE
    LOG_OUT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/log-out"
)
add_test(NAME logout COMMAND tst_logout)

qt_add_executable(tst_foreignwindowlist tests/tst_foreignwindowlist.cpp ${launcher_window_sources})
target_include_directories(tst_foreignwindowlist PRIVATE src)
target_link_libraries(tst_foreignwindowlist PRIVATE Qt6::Gui Qt6::Qml Qt6::Test cairn-foreign-toplevel)
add_test(NAME foreignwindowlist COMMAND tst_foreignwindowlist)

qt_add_executable(tst_tokens_loaded tests/tst_tokens_loaded.cpp)
target_link_libraries(tst_tokens_loaded PRIVATE Qt6::Quick Qt6::Test CairnBrand)
target_compile_definitions(tst_tokens_loaded PRIVATE
    CAIRN_TOKENS_JSON="${PROJECT_SOURCE_DIR}/brand/tokens.json"
)
add_test(NAME tokens_loaded COMMAND tst_tokens_loaded)

qt_add_executable(tst_doorsfromtiles tests/tst_doorsfromtiles.cpp
    ${launcher_model_sources} ${launcher_terminal_sources})
target_include_directories(tst_doorsfromtiles PRIVATE src)
target_link_libraries(tst_doorsfromtiles PRIVATE Qt6::Qml Qt6::Test footpath footpathplugin)
add_test(NAME doorsfromtiles COMMAND tst_doorsfromtiles)

# kidscan's own manifest, read by the launcher (#97). The first test scans a
# synthetic Steam library and writes the file; CTest always runs it before
# the second, which reads it, because one sets up the fixture the other needs.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(kidscan_manifest "${CMAKE_CURRENT_BINARY_DIR}/kidscan-roundtrip.json")
add_test(NAME kidscan_writes_manifest
    COMMAND Python3::Interpreter
        "${PROJECT_SOURCE_DIR}/tools/kidscan/tests/roundtrip_manifest.py" "${kidscan_manifest}"
)
set_tests_properties(kidscan_writes_manifest PROPERTIES FIXTURES_SETUP kidscan_manifest)
qt_add_executable(tst_kidscanmanifest tests/tst_kidscanmanifest.cpp ${launcher_model_sources})
target_include_directories(tst_kidscanmanifest PRIVATE src)
target_link_libraries(tst_kidscanmanifest PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME launcher_reads_kidscan COMMAND tst_kidscanmanifest)
set_tests_properties(launcher_reads_kidscan PROPERTIES
    FIXTURES_REQUIRED kidscan_manifest
    ENVIRONMENT "KIDSCAN_MANIFEST=${kidscan_manifest}"
)

# Compile the same module into the Quick Test host, including the real Main.qml.
qt_add_executable(tst_navigation tests/tst_navigation.cpp)
qt_add_qml_module(tst_navigation
    URI Cairn.Launcher
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/Cairn/Launcher"
    QML_FILES ${launcher_qml_files}
    SOURCES ${launcher_model_sources} ${launcher_window_sources} ${launcher_terminal_sources}
)
target_include_directories(tst_navigation PRIVATE src)
target_link_libraries(tst_navigation PRIVATE Qt6::Gui Qt6::QuickTest CairnBrand cairn-foreign-toplevel footpath footpathplugin)
target_compile_definitions(tst_navigation PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
add_test(NAME navigation COMMAND tst_navigation)

set_tests_properties(tilemodel manifest applauncher appscope launchwindows giveupwatch steamatlogin gridscroller closerequest logout foreignwindowlist tokens_loaded doorsfromtiles navigation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
)
