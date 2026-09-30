# SPDX-License-Identifier: Apache-2.0
# The login screen (ADR-0020): an SDDM theme in theme/, and the small C++ QML
# module it imports, Cairn.Greeter, which says what kind of family member each
# account is. SDDM loads the module as a plugin from its import path.

set(greeter_sources
    src/AccountKind.h src/AccountKind.cpp
    src/Account.h src/Account.cpp
    src/FamilyModel.h src/FamilyModel.cpp
)
set(greeter_theme_files
    theme/Main.qml theme/FamilyTile.qml theme/PasswordRow.qml theme/TextButton.qml
)

qt_add_library(cairngreeter SHARED)
qt_add_qml_module(cairngreeter
    URI Cairn.Greeter
    VERSION 1.0
    SOURCES ${greeter_sources}
)
target_include_directories(cairngreeter PRIVATE src)
target_link_libraries(cairngreeter PRIVATE Qt6::Qml Qt6::Core)
# The plugin finds the library beside it once both are installed together.
set_target_properties(cairngreeterplugin PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE)

qt_add_lupdate(
    SOURCES ${greeter_theme_files}
    TS_FILES i18n/greeter_en.ts
    LUPDATE_TARGET greeter_lupdate
)

qt_add_executable(tst_accountkind tests/tst_accountkind.cpp src/AccountKind.h src/AccountKind.cpp)
target_include_directories(tst_accountkind PRIVATE src)
target_link_libraries(tst_accountkind PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME accountkind COMMAND tst_accountkind)

qt_add_executable(tst_familymodel tests/tst_familymodel.cpp ${greeter_sources})
target_include_directories(tst_familymodel PRIVATE src)
target_link_libraries(tst_familymodel PRIVATE Qt6::Gui Qt6::Qml Qt6::Test)
add_test(NAME familymodel COMMAND tst_familymodel)

# The real theme, loaded from the source tree with a stand-in for SDDM and a
# table of accounts. The module is compiled into the test host, as the
# launcher's navigation test does.
qt_add_executable(tst_loginscreen tests/tst_loginscreen.cpp tests/StandInSddm.h tests/StandInSddm.cpp)
qt_add_qml_module(tst_loginscreen
    URI Cairn.Greeter
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/Cairn/Greeter"
    SOURCES ${greeter_sources}
)
target_include_directories(tst_loginscreen PRIVATE src tests)
target_link_libraries(tst_loginscreen PRIVATE Qt6::Gui Qt6::QuickTest CairnBrand)
target_compile_definitions(tst_loginscreen PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
add_test(NAME loginscreen COMMAND tst_loginscreen)

set_tests_properties(accountkind familymodel loginscreen PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_CURRENT_BINARY_DIR}/tests:${QT_QML_OUTPUT_DIRECTORY}"
)
