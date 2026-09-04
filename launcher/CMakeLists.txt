# SPDX-License-Identifier: Apache-2.0
find_package(Qt6 REQUIRED COMPONENTS LinguistTools)

set(launcher_model_sources
    src/TileModel.h src/TileModel.cpp
    src/Manifest.h src/Manifest.cpp
    src/AppLauncher.h src/AppLauncher.cpp
)
set(launcher_qml_files qml/Main.qml qml/Tile.qml qml/GrownUpScreen.qml)
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/Tile.qml PROPERTIES QT_RESOURCE_ALIAS Tile.qml)
set_source_files_properties(qml/GrownUpScreen.qml PROPERTIES QT_RESOURCE_ALIAS GrownUpScreen.qml)

qt_add_executable(cairn-launcher src/main.cpp)
qt_add_qml_module(cairn-launcher
    URI Cairn.Launcher
    QML_FILES ${launcher_qml_files}
    SOURCES ${launcher_model_sources}
)
target_include_directories(cairn-launcher PRIVATE src)
target_link_libraries(cairn-launcher PRIVATE Qt6::Quick CairnBrand)

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

qt_add_executable(tst_tokens_loaded tests/tst_tokens_loaded.cpp)
target_link_libraries(tst_tokens_loaded PRIVATE Qt6::Quick Qt6::Test CairnBrand)
target_compile_definitions(tst_tokens_loaded PRIVATE
    CAIRN_TOKENS_JSON="${PROJECT_SOURCE_DIR}/brand/tokens.json"
)
add_test(NAME tokens_loaded COMMAND tst_tokens_loaded)

# Compile the same module into the Quick Test host, including the real Main.qml.
qt_add_executable(tst_navigation tests/tst_navigation.cpp)
qt_add_qml_module(tst_navigation
    URI Cairn.Launcher
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/Cairn/Launcher"
    QML_FILES ${launcher_qml_files}
    SOURCES ${launcher_model_sources}
)
target_include_directories(tst_navigation PRIVATE src)
target_link_libraries(tst_navigation PRIVATE Qt6::QuickTest CairnBrand)
target_compile_definitions(tst_navigation PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
add_test(NAME navigation COMMAND tst_navigation)

set_tests_properties(tilemodel manifest applauncher tokens_loaded navigation PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
)
