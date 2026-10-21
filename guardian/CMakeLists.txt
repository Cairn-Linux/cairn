# SPDX-License-Identifier: Apache-2.0
# The Guardian's side of Cairn (P1-5). So far one window: a child's own Steam,
# opened from the Guardian's own login (ADR-0032).
find_package(Qt6 REQUIRED COMPONENTS DBus LinguistTools)

set(guardian_sources
    src/AccountName.h src/AccountName.cpp
    src/SteamVisit.h src/SteamVisit.cpp
    src/OwnSteam.h src/OwnSteam.cpp
    src/ScreenLockWatch.h src/ScreenLockWatch.cpp
    src/SteamForChild.h src/SteamForChild.cpp
)
set(guardian_qml_files qml/Main.qml qml/TextButton.qml)
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/TextButton.qml PROPERTIES QT_RESOURCE_ALIAS TextButton.qml)

qt_add_executable(cairn-steam-for src/main.cpp)
qt_add_qml_module(cairn-steam-for
    URI Cairn.Guardian
    QML_FILES ${guardian_qml_files}
    SOURCES ${guardian_sources}
)
target_include_directories(cairn-steam-for PRIVATE src)
target_link_libraries(cairn-steam-for PRIVATE Qt6::Quick Qt6::DBus CairnBrand)

# The root helper it asks pkexec to run, where this build installs it.
target_compile_definitions(cairn-steam-for PRIVATE
    CAIRN_STEAM_VISIT="${CAIRN_FULL_LIBEXECDIR}/cairn-steam-visit"
)

# Only a Guardian may run the window or its wrapper, mode 0750 with the
# group cairn-guardian. The group does not exist when the image is built
# here, so the image sets it in a later step (ADR-0038).
set_target_properties(cairn-steam-for PROPERTIES INSTALL_RPATH "${CAIRN_FULL_LIBDIR}")
cairn_permissions(750 guardian_only)
install(TARGETS cairn-steam-for RUNTIME DESTINATION "${CAIRN_LIBEXECDIR}"
    PERMISSIONS ${guardian_only})
configure_file(bin/cairn-steam-for.in bin/cairn-steam-for @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bin/cairn-steam-for" DESTINATION "${CMAKE_INSTALL_BINDIR}"
    PERMISSIONS ${guardian_only})

qt_add_lupdate(
    SOURCE_TARGETS cairn-steam-for
    TS_FILES i18n/guardian_en.ts
    LUPDATE_TARGET guardian_lupdate
)

# Stand-ins for pkexec with the root helper behind it, and for Steam.
set(guardian_fixtures "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")

qt_add_executable(tst_steamvisit tests/tst_steamvisit.cpp src/SteamVisit.h src/SteamVisit.cpp)
target_include_directories(tst_steamvisit PRIVATE src)
target_link_libraries(tst_steamvisit PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_steamvisit PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME steamvisit COMMAND tst_steamvisit)

qt_add_executable(tst_ownsteam tests/tst_ownsteam.cpp src/OwnSteam.h src/OwnSteam.cpp)
target_include_directories(tst_ownsteam PRIVATE src)
target_link_libraries(tst_ownsteam PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_ownsteam PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME ownsteam COMMAND tst_ownsteam)

# On a session bus of its own, so a real screen lock never reaches it.
find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
qt_add_executable(tst_screenlockwatch tests/tst_screenlockwatch.cpp
    src/ScreenLockWatch.h src/ScreenLockWatch.cpp)
target_include_directories(tst_screenlockwatch PRIVATE src)
target_link_libraries(tst_screenlockwatch PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_test(NAME screenlockwatch COMMAND "${DBUS_RUN_SESSION}" -- $<TARGET_FILE:tst_screenlockwatch>)

qt_add_executable(tst_steamforchild tests/tst_steamforchild.cpp ${guardian_sources})
target_include_directories(tst_steamforchild PRIVATE src)
target_link_libraries(tst_steamforchild PRIVATE Qt6::Qml Qt6::DBus Qt6::Test)
target_compile_definitions(tst_steamforchild PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME steamforchild COMMAND "${DBUS_RUN_SESSION}" -- $<TARGET_FILE:tst_steamforchild>)

# The real window, compiled into the Quick Test host as the launcher's
# navigation test does, with a SteamForChild that runs the stand-ins.
qt_add_executable(tst_steamwindow tests/tst_steamwindow.cpp)
qt_add_qml_module(tst_steamwindow
    URI Cairn.Guardian
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/Cairn/Guardian"
    QML_FILES ${guardian_qml_files}
    SOURCES ${guardian_sources}
)
target_include_directories(tst_steamwindow PRIVATE src)
target_link_libraries(tst_steamwindow PRIVATE Qt6::Gui Qt6::DBus Qt6::QuickTest CairnBrand)
target_compile_definitions(tst_steamwindow PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    FIXTURES="${guardian_fixtures}"
)
add_test(NAME steamwindow COMMAND "${DBUS_RUN_SESSION}" -- $<TARGET_FILE:tst_steamwindow>)

set_tests_properties(steamvisit ownsteam screenlockwatch steamforchild steamwindow PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
)
