# SPDX-License-Identifier: Apache-2.0
# The Guardian's side of Cairn (P1-5): a window for a child's own Steam
# opened from the Guardian's own login (ADR-0032), cairn-family, the root
# command that makes children and sets their levels (ADR-0039), and the
# Guardian tool, "Family" in the menu, that runs it (ADR-0040).
find_package(Qt6 REQUIRED COMPONENTS DBus LinguistTools)

set(guardian_sources
    src/SteamVisit.h src/SteamVisit.cpp
    src/OwnSteam.h src/OwnSteam.cpp
    src/ScreenLockWatch.h src/ScreenLockWatch.cpp
    src/SteamForChild.h src/SteamForChild.cpp
)
set(guardian_qml_files qml/Main.qml qml/SteamForWords.qml qml/TextButton.qml)
set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/SteamForWords.qml PROPERTIES QT_RESOURCE_ALIAS SteamForWords.qml)
set_source_files_properties(qml/TextButton.qml PROPERTIES QT_RESOURCE_ALIAS TextButton.qml)

qt_add_executable(cairn-steam-for src/main.cpp)
qt_add_qml_module(cairn-steam-for
    URI Cairn.Guardian
    QML_FILES ${guardian_qml_files}
    SOURCES ${guardian_sources}
)
target_include_directories(cairn-steam-for PRIVATE src)
target_link_libraries(cairn-steam-for PRIVATE Qt6::Quick Qt6::DBus CairnBrand cairn-accounts)

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

# cairn-family: no window, only Qt Core, run as root by pkexec or sudo.
# create-child and set-level change accounts (ADR-0039); save-games gives
# children their games, and log-out logs a child out first (ADR-0041).
set(family_sources
    src/Refusal.h src/Refusal.cpp
    src/Caller.h src/Caller.cpp
    src/FamilyRequest.h src/FamilyRequest.cpp
    src/SystemTool.h src/SystemTool.cpp
    src/PasswordLock.h src/PasswordLock.cpp
    src/SteamEntry.h src/SteamEntry.cpp
    src/ReadableFile.h src/ReadableFile.cpp
    src/RootFolder.h src/RootFolder.cpp
    src/FamilyCommand.h src/FamilyCommand.cpp
)
set(games_save_sources
    src/GamesRequest.h src/GamesRequest.cpp
    src/GameCatalogue.h src/GameCatalogue.cpp
    src/CatalogueRefresh.h src/CatalogueRefresh.cpp
    src/ChildGameList.h src/ChildGameList.cpp
    src/ChildGamesFile.h src/ChildGamesFile.cpp
    src/SystemJob.h src/SystemJob.cpp
    src/CopyJob.h src/CopyJob.cpp
    src/ChildSave.h src/ChildSave.cpp
    src/GamesSave.h src/GamesSave.cpp
    src/LogOutCommand.h src/LogOutCommand.cpp
)
qt_add_executable(cairn-family src/familymain.cpp ${family_sources} ${games_save_sources})
target_include_directories(cairn-family PRIVATE src)
target_link_libraries(cairn-family PRIVATE Qt6::Core cairn-accounts)
# The window each child's menu entry names, where this build installs it,
# and the folder for the entries. That folder is the machine's own in both
# kinds of install: on Bazzite /usr/local is a link to /var/usrlocal, which
# the image may not ship files in but the family's computer may write.
target_compile_definitions(cairn-family PRIVATE
    CAIRN_STEAM_FOR="${CMAKE_INSTALL_FULL_BINDIR}/cairn-steam-for"
    CAIRN_APPLICATIONS="/usr/local/share/applications"
)
# save-games runs the visit helper's copy, the picture step and kidscan,
# where this build installs them, and log-out the helper's logout.
# systemd-run, systemd-inhibit and python3 are the system's own, at their
# fixed paths.
target_compile_definitions(cairn-family PRIVATE
    CAIRN_STEAM_VISIT="${CAIRN_FULL_LIBEXECDIR}/cairn-steam-visit"
    CAIRN_GAME_PICTURES="${CAIRN_FULL_LIBEXECDIR}/cairn-game-pictures"
    CAIRN_KIDSCAN="${CAIRN_FULL_LIBEXECDIR}/kidscan.py"
)
install(TARGETS cairn-family RUNTIME DESTINATION "${CAIRN_LIBEXECDIR}")

# The Guardian tool: its own program and QML module, so the Steam window
# stays as it is. It shares the Steam window's button, and checks what it
# asks for with cairn-family's own rule before it asks for a password.
# Qt's D-Bus module asks logind, without root, which children are logged
# in, and listens for logins coming and going (ADR-0041).
set(family_window_sources
    src/Refusal.h src/Refusal.cpp
    src/FamilyRequest.h src/FamilyRequest.cpp
    src/LoginName.h src/LoginName.cpp
    src/ChildList.h src/ChildList.cpp
    src/PkexecAnswer.h src/PkexecAnswer.cpp
    src/FamilyChange.h src/FamilyChange.cpp
    src/ChildrenPage.h src/ChildrenPage.cpp
    src/GameCatalogue.h src/GameCatalogue.cpp
    src/GamePicture.h src/GamePicture.cpp
    src/GameShelf.h src/GameShelf.cpp
    src/GamesRequest.h src/GamesRequest.cpp
    src/GameChoices.h src/GameChoices.cpp
    src/SystemTool.h src/SystemTool.cpp
    src/ReadableFile.h src/ReadableFile.cpp
    src/RootFolder.h src/RootFolder.cpp
    src/SteamWindowStart.h src/SteamWindowStart.cpp
    src/SaveRun.h src/SaveRun.cpp
    src/SaveReport.h src/SaveReport.cpp
    src/SaveResults.h src/SaveResults.cpp
    src/GamesSaving.h src/GamesSaving.cpp
    src/LoginSessions.h src/LoginSessions.cpp
    src/LoginWatch.h src/LoginWatch.cpp
    src/ChildLogOut.h src/ChildLogOut.cpp
    src/GamesPage.h src/GamesPage.cpp
)
set(family_window_qml_files
    qml/FamilyWindow.qml
    qml/ChildrenView.qml
    qml/GamesView.qml
    qml/ChildGames.qml
    qml/ChildChoice.qml
    qml/GameCard.qml
    qml/SaveOutcome.qml
    qml/SaveResultWords.qml
    qml/SaveProgress.qml
    qml/LoggedInCover.qml
    qml/ChildRow.qml
    qml/AddChildForm.qml
    qml/LabelledField.qml
    qml/LevelChoice.qml
    qml/TextButton.qml
)
foreach(file IN LISTS family_window_qml_files)
    cmake_path(GET file FILENAME name)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()

qt_add_executable(cairn-guardian src/guardianmain.cpp)
qt_add_qml_module(cairn-guardian
    URI Cairn.Family
    QML_FILES ${family_window_qml_files}
    SOURCES ${family_window_sources}
)
target_include_directories(cairn-guardian PRIVATE src)
target_link_libraries(cairn-guardian PRIVATE Qt6::Quick Qt6::DBus CairnBrand cairn-accounts)
# cairn-family, and the Steam window the Games page opens for a child.
target_compile_definitions(cairn-guardian PRIVATE
    CAIRN_FAMILY="${CAIRN_FULL_LIBEXECDIR}/cairn-family"
    CAIRN_STEAM_FOR="${CMAKE_INSTALL_FULL_BINDIR}/cairn-steam-for"
)
# Only a Guardian may run it, as the Steam window.
set_target_properties(cairn-guardian PROPERTIES INSTALL_RPATH "${CAIRN_FULL_LIBDIR}")
install(TARGETS cairn-guardian RUNTIME DESTINATION "${CAIRN_LIBEXECDIR}"
    PERMISSIONS ${guardian_only})
configure_file(bin/cairn-guardian.in bin/cairn-guardian @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/bin/cairn-guardian" DESTINATION "${CMAKE_INSTALL_BINDIR}"
    PERMISSIONS ${guardian_only})
configure_file(applications/cairn-guardian.desktop.in applications/cairn-guardian.desktop @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/applications/cairn-guardian.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

qt_add_lupdate(
    SOURCE_TARGETS cairn-steam-for cairn-family cairn-guardian
    TS_FILES i18n/guardian_en.ts
    LUPDATE_TARGET guardian_lupdate
)

# Stand-ins for pkexec with the root helper behind it, for Steam, and for
# the system's account tools.
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
target_link_libraries(tst_steamforchild PRIVATE Qt6::Qml Qt6::DBus Qt6::Test cairn-accounts)
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
target_link_libraries(tst_steamwindow PRIVATE Qt6::Gui Qt6::DBus Qt6::QuickTest CairnBrand
    cairn-accounts)
target_compile_definitions(tst_steamwindow PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    FIXTURES="${guardian_fixtures}"
)
# Each Quick Test host runs only its own file from the tests folder.
add_test(NAME steamwindow COMMAND "${DBUS_RUN_SESSION}" -- $<TARGET_FILE:tst_steamwindow>
    -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_steamwindow.qml")

# cairn-family with stand-ins for the account tools and a table of accounts,
# so no test needs root or changes a real account.
qt_add_executable(tst_familycommand tests/tst_familycommand.cpp ${family_sources})
target_include_directories(tst_familycommand PRIVATE src)
target_link_libraries(tst_familycommand PRIVATE Qt6::Core Qt6::Test cairn-accounts)
target_compile_definitions(tst_familycommand PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME familycommand COMMAND tst_familycommand)

# save-games, piece by piece, then whole, with stand-ins for systemd-run,
# the visit helper and Python, so no test needs root or starts a service.
qt_add_executable(tst_caller tests/tst_caller.cpp src/Caller.h src/Caller.cpp
    src/Refusal.h src/Refusal.cpp)
target_include_directories(tst_caller PRIVATE src)
target_link_libraries(tst_caller PRIVATE Qt6::Core Qt6::Test cairn-accounts)
add_test(NAME caller COMMAND tst_caller)

qt_add_executable(tst_gamesrequest tests/tst_gamesrequest.cpp src/GamesRequest.h
    src/GamesRequest.cpp src/Refusal.h src/Refusal.cpp)
target_include_directories(tst_gamesrequest PRIVATE src)
target_link_libraries(tst_gamesrequest PRIVATE Qt6::Core Qt6::Test)
add_test(NAME gamesrequest COMMAND tst_gamesrequest)

qt_add_executable(tst_childgamelist tests/tst_childgamelist.cpp src/ChildGameList.h
    src/ChildGameList.cpp src/GameCatalogue.h src/GameCatalogue.cpp)
target_include_directories(tst_childgamelist PRIVATE src)
target_link_libraries(tst_childgamelist PRIVATE Qt6::Core Qt6::Test)
add_test(NAME childgamelist COMMAND tst_childgamelist)

qt_add_executable(tst_childgamesfile tests/tst_childgamesfile.cpp src/ChildGamesFile.h
    src/ChildGamesFile.cpp src/GameCatalogue.h src/GameCatalogue.cpp src/ReadableFile.h
    src/ReadableFile.cpp src/RootFolder.h src/RootFolder.cpp src/SystemTool.h src/SystemTool.cpp)
target_include_directories(tst_childgamesfile PRIVATE src)
target_link_libraries(tst_childgamesfile PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_childgamesfile PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME childgamesfile COMMAND tst_childgamesfile)

qt_add_executable(tst_cataloguerefresh tests/tst_cataloguerefresh.cpp src/CatalogueRefresh.h
    src/CatalogueRefresh.cpp src/GameCatalogue.h src/GameCatalogue.cpp src/ReadableFile.h
    src/ReadableFile.cpp src/SystemTool.h src/SystemTool.cpp)
target_include_directories(tst_cataloguerefresh PRIVATE src)
target_link_libraries(tst_cataloguerefresh PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_cataloguerefresh PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME cataloguerefresh COMMAND tst_cataloguerefresh)

qt_add_executable(tst_copyjob tests/tst_copyjob.cpp src/CopyJob.h src/CopyJob.cpp
    src/SystemJob.h src/SystemJob.cpp)
target_include_directories(tst_copyjob PRIVATE src)
target_link_libraries(tst_copyjob PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_copyjob PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME copyjob COMMAND tst_copyjob)

qt_add_executable(tst_logoutcommand tests/tst_logoutcommand.cpp src/LogOutCommand.h
    src/LogOutCommand.cpp src/SystemJob.h src/SystemJob.cpp src/Caller.h src/Caller.cpp
    src/Refusal.h src/Refusal.cpp)
target_include_directories(tst_logoutcommand PRIVATE src)
target_link_libraries(tst_logoutcommand PRIVATE Qt6::Core Qt6::Test cairn-accounts)
target_compile_definitions(tst_logoutcommand PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME logoutcommand COMMAND tst_logoutcommand)

qt_add_executable(tst_gamessave tests/tst_gamessave.cpp ${family_sources} ${games_save_sources})
target_include_directories(tst_gamessave PRIVATE src)
target_link_libraries(tst_gamessave PRIVATE Qt6::Core Qt6::Test cairn-accounts)
target_compile_definitions(tst_gamessave PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME gamessave COMMAND tst_gamessave)

qt_add_executable(tst_steamentry tests/tst_steamentry.cpp src/SteamEntry.h src/SteamEntry.cpp)
target_include_directories(tst_steamentry PRIVATE src)
target_link_libraries(tst_steamentry PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_steamentry PRIVATE
    PROVISION_SCRIPT="${PROJECT_SOURCE_DIR}/provision/cairn-provision.sh"
)
add_test(NAME steamentry COMMAND tst_steamentry)

# The Guardian tool's pieces, each with its own test, cairn-family behind a
# stand-in for pkexec, and a table of accounts in place of the system's.
qt_add_executable(tst_loginname tests/tst_loginname.cpp src/LoginName.h src/LoginName.cpp)
target_include_directories(tst_loginname PRIVATE src)
target_link_libraries(tst_loginname PRIVATE Qt6::Core Qt6::Test)
add_test(NAME loginname COMMAND tst_loginname)

qt_add_executable(tst_childlist tests/tst_childlist.cpp src/ChildList.h src/ChildList.cpp)
target_include_directories(tst_childlist PRIVATE src)
target_link_libraries(tst_childlist PRIVATE Qt6::Core Qt6::Test cairn-accounts)
add_test(NAME childlist COMMAND tst_childlist)

qt_add_executable(tst_familychange tests/tst_familychange.cpp
    src/FamilyChange.h src/FamilyChange.cpp src/PkexecAnswer.h src/PkexecAnswer.cpp)
target_include_directories(tst_familychange PRIVATE src)
target_link_libraries(tst_familychange PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_familychange PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME familychange COMMAND tst_familychange)

qt_add_executable(tst_saverun tests/tst_saverun.cpp src/SaveRun.h src/SaveRun.cpp
    src/PkexecAnswer.h src/PkexecAnswer.cpp)
target_include_directories(tst_saverun PRIVATE src)
target_link_libraries(tst_saverun PRIVATE Qt6::Core Qt6::Test)
target_compile_definitions(tst_saverun PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME saverun COMMAND tst_saverun)

qt_add_executable(tst_savereport tests/tst_savereport.cpp src/SaveReport.h src/SaveReport.cpp)
target_include_directories(tst_savereport PRIVATE src)
target_link_libraries(tst_savereport PRIVATE Qt6::Core Qt6::Test)
add_test(NAME savereport COMMAND tst_savereport)

# Who is logged in: the visit helper's rule on sessions given here, then
# the system's own logind, when the test machine has one.
qt_add_executable(tst_loginsessions tests/tst_loginsessions.cpp src/LoginSessions.h
    src/LoginSessions.cpp)
target_include_directories(tst_loginsessions PRIVATE src)
target_link_libraries(tst_loginsessions PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_test(NAME loginsessions COMMAND tst_loginsessions)

# On a session bus of its own, with a stand-in for logind's signals.
qt_add_executable(tst_loginwatch tests/tst_loginwatch.cpp src/LoginWatch.h src/LoginWatch.cpp)
target_include_directories(tst_loginwatch PRIVATE src)
target_link_libraries(tst_loginwatch PRIVATE Qt6::Core Qt6::DBus Qt6::Test)
add_test(NAME loginwatch COMMAND "${DBUS_RUN_SESSION}" -- $<TARGET_FILE:tst_loginwatch>)

qt_add_executable(tst_childlogout tests/tst_childlogout.cpp src/ChildLogOut.h
    src/ChildLogOut.cpp src/SaveRun.h src/SaveRun.cpp src/PkexecAnswer.h src/PkexecAnswer.cpp)
target_include_directories(tst_childlogout PRIVATE src)
target_link_libraries(tst_childlogout PRIVATE Qt6::Qml Qt6::Test)
target_compile_definitions(tst_childlogout PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME childlogout COMMAND tst_childlogout)

qt_add_executable(tst_gamechoices tests/tst_gamechoices.cpp src/GameChoices.h
    src/GameChoices.cpp)
target_include_directories(tst_gamechoices PRIVATE src)
target_link_libraries(tst_gamechoices PRIVATE Qt6::Core Qt6::Test)
add_test(NAME gamechoices COMMAND tst_gamechoices)

qt_add_executable(tst_childrenpage tests/tst_childrenpage.cpp ${family_window_sources})
target_include_directories(tst_childrenpage PRIVATE src)
target_link_libraries(tst_childrenpage PRIVATE Qt6::Qml Qt6::DBus Qt6::Test cairn-accounts)
target_compile_definitions(tst_childrenpage PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME childrenpage COMMAND tst_childrenpage)

qt_add_executable(tst_gamepicture tests/tst_gamepicture.cpp src/GamePicture.h
    src/GamePicture.cpp)
target_include_directories(tst_gamepicture PRIVATE src)
target_link_libraries(tst_gamepicture PRIVATE Qt6::Core Qt6::Test)
add_test(NAME gamepicture COMMAND tst_gamepicture)

qt_add_executable(tst_gameshelf tests/tst_gameshelf.cpp src/GameShelf.h src/GameShelf.cpp
    src/GameCatalogue.h src/GameCatalogue.cpp src/GamePicture.h src/GamePicture.cpp)
target_include_directories(tst_gameshelf PRIVATE src)
target_link_libraries(tst_gameshelf PRIVATE Qt6::Core Qt6::Test)
add_test(NAME gameshelf COMMAND tst_gameshelf)

qt_add_executable(tst_gamespage tests/tst_gamespage.cpp ${family_window_sources})
target_include_directories(tst_gamespage PRIVATE src)
target_link_libraries(tst_gamespage PRIVATE Qt6::Qml Qt6::DBus Qt6::Test cairn-accounts)
target_compile_definitions(tst_gamespage PRIVATE FIXTURES="${guardian_fixtures}")
add_test(NAME gamespage COMMAND tst_gamespage)

# The real window in the Quick Test host, as for the Steam window.
qt_add_executable(tst_familywindow tests/tst_familywindow.cpp)
qt_add_qml_module(tst_familywindow
    URI Cairn.Family
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/Cairn/Family"
    QML_FILES ${family_window_qml_files}
    SOURCES ${family_window_sources}
)
target_include_directories(tst_familywindow PRIVATE src)
target_link_libraries(tst_familywindow PRIVATE Qt6::Gui Qt6::DBus Qt6::QuickTest CairnBrand
    cairn-accounts)
target_compile_definitions(tst_familywindow PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests"
    FIXTURES="${guardian_fixtures}"
)
add_test(NAME familywindow COMMAND tst_familywindow
    -input "${CMAKE_CURRENT_SOURCE_DIR}/tests/tst_familywindow.qml")

set_tests_properties(steamvisit ownsteam screenlockwatch steamforchild steamwindow familycommand
    caller gamesrequest childgamelist childgamesfile cataloguerefresh copyjob logoutcommand gamessave
    steamentry
    loginname childlist familychange saverun savereport loginsessions loginwatch childlogout
    gamechoices childrenpage
    gamepicture gameshelf gamespage familywindow PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QML_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
)
