# SPDX-License-Identifier: Apache-2.0
# The login screen (ADR-0020): an SDDM theme in theme/, and the small C++ QML
# module it imports, Cairn.Greeter, which says what kind of family member each
# account is, by the family's one rule in ../accounts. SDDM loads the module
# as a plugin from its import path.

# What the screen needs to say an account is locked (ADR-0052), named on
# its own for the tests.
set(greeter_lock_sources
    src/FaillockSettings.h src/FaillockSettings.cpp
    src/TallyFile.h src/TallyFile.cpp
    src/LockRule.h src/LockRule.cpp
    src/AccountLock.h src/AccountLock.cpp
)
set(greeter_sources
    src/AccountKind.h src/AccountKind.cpp
    src/FamilyModel.h src/FamilyModel.cpp
    src/FailedLogin.h src/FailedLogin.cpp
    ${greeter_lock_sources}
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
target_link_libraries(cairngreeter PRIVATE Qt6::Qml Qt6::Core cairn-accounts)
# The plugin finds the library beside it once both are installed together.
set_target_properties(cairngreeterplugin PROPERTIES INSTALL_RPATH "$ORIGIN" BUILD_WITH_INSTALL_RPATH TRUE)

# The theme where SDDM looks for one, and both modules it imports in
# Cairn's own QML directory, which the SDDM settings name (ADR-0038).
foreach(file ${greeter_theme_files} theme/metadata.desktop theme/theme.conf theme/mark-on-ink.svg)
    cairn_install_file(SOURCE ${file} DESTINATION "${CAIRN_SDDM_THEMESDIR}/cairn" MODE 644)
endforeach()
# The tokens as plain QML, so the greeter needs no brand library.
foreach(file qmldir Tokens.qml)
    cairn_install_file(SOURCE ../brand/qml/Cairn/Brand/${file} DESTINATION "${CAIRN_QMLDIR}/Cairn/Brand"
        MODE 644)
endforeach()
install(TARGETS cairngreeter cairngreeterplugin LIBRARY DESTINATION "${CAIRN_QMLDIR}/Cairn/Greeter")
install(FILES "${QT_QML_OUTPUT_DIRECTORY}/Cairn/Greeter/qmldir"
    "${QT_QML_OUTPUT_DIRECTORY}/Cairn/Greeter/cairngreeter.qmltypes"
    DESTINATION "${CAIRN_QMLDIR}/Cairn/Greeter")

qt_add_lupdate(
    SOURCES ${greeter_theme_files}
    TS_FILES i18n/greeter_en.ts
    LUPDATE_TARGET greeter_lupdate
)

qt_add_executable(tst_accountkind tests/tst_accountkind.cpp src/AccountKind.h src/AccountKind.cpp)
target_include_directories(tst_accountkind PRIVATE src)
target_link_libraries(tst_accountkind PRIVATE Qt6::Qml Qt6::Test cairn-accounts)
add_test(NAME accountkind COMMAND tst_accountkind)

qt_add_executable(tst_familymodel tests/tst_familymodel.cpp ${greeter_sources})
target_include_directories(tst_familymodel PRIVATE src)
target_link_libraries(tst_familymodel PRIVATE Qt6::Gui Qt6::Qml Qt6::Test cairn-accounts)
add_test(NAME familymodel COMMAND tst_familymodel)

qt_add_executable(tst_failedlogin tests/tst_failedlogin.cpp src/FailedLogin.h src/FailedLogin.cpp
    ${greeter_lock_sources} tests/StandInFaillock.h tests/StandInFaillock.cpp)
target_include_directories(tst_failedlogin PRIVATE src tests)
target_link_libraries(tst_failedlogin PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME failedlogin COMMAND tst_failedlogin)

qt_add_executable(tst_faillocksettings tests/tst_faillocksettings.cpp
    src/FaillockSettings.h src/FaillockSettings.cpp)
target_include_directories(tst_faillocksettings PRIVATE src)
target_link_libraries(tst_faillocksettings PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_faillocksettings PRIVATE
    CAIRN_FAILLOCK_CONF="${PROJECT_SOURCE_DIR}/session/security/faillock.conf")
add_test(NAME faillocksettings COMMAND tst_faillocksettings)

qt_add_executable(tst_tallyfile tests/tst_tallyfile.cpp src/TallyFile.h src/TallyFile.cpp
    tests/StandInFaillock.h tests/StandInFaillock.cpp)
target_include_directories(tst_tallyfile PRIVATE src tests)
target_link_libraries(tst_tallyfile PRIVATE Qt6::Core Qt6::Test)
add_test(NAME tallyfile COMMAND tst_tallyfile)

qt_add_executable(tst_lockrule tests/tst_lockrule.cpp src/LockRule.h src/LockRule.cpp
    src/FaillockSettings.h)
target_include_directories(tst_lockrule PRIVATE src)
target_link_libraries(tst_lockrule PRIVATE Qt6::Core Qt6::Test)
add_test(NAME lockrule COMMAND tst_lockrule)

qt_add_executable(tst_accountlock tests/tst_accountlock.cpp ${greeter_lock_sources}
    tests/StandInFaillock.h tests/StandInFaillock.cpp)
target_include_directories(tst_accountlock PRIVATE src tests)
target_link_libraries(tst_accountlock PRIVATE Qt6::Qml Qt6::Test)
add_test(NAME accountlock COMMAND tst_accountlock)

# The real theme, loaded from the source tree with a stand-in for SDDM and a
# table of accounts. The module is compiled into the test host, as the
# launcher's navigation test does.
qt_add_executable(tst_loginscreen tests/tst_loginscreen.cpp tests/StandInSddm.h tests/StandInSddm.cpp
    tests/StandInFaillock.h tests/StandInFaillock.cpp)
qt_add_qml_module(tst_loginscreen
    URI Cairn.Greeter
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/Cairn/Greeter"
    SOURCES ${greeter_sources}
)
target_include_directories(tst_loginscreen PRIVATE src tests)
target_link_libraries(tst_loginscreen PRIVATE Qt6::Gui Qt6::QuickTest CairnBrand cairn-accounts)
target_compile_definitions(tst_loginscreen PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
)
add_test(NAME loginscreen COMMAND tst_loginscreen)

set_tests_properties(accountkind familymodel failedlogin faillocksettings tallyfile lockrule
    accountlock loginscreen PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${CMAKE_CURRENT_BINARY_DIR}/tests:${QT_QML_OUTPUT_DIRECTORY}"
)
