# SPDX-License-Identifier: Apache-2.0
# Who each account is in the family, read from the system's account database
# (ADR-0011, ADR-0023). One rule for every program that asks: the login
# screen and the command that makes children (ADR-0039). Also faillock's
# settings, which the login screen and Family both put into words (ADR-0052,
# ADR-0054). A static library,
# compiled into each of them, so nothing new is installed. The login
# screen's module is a shared library, so the code must be position
# independent.
add_library(cairn-accounts STATIC
    src/Account.h src/Account.cpp
    src/FamilyRole.h src/FamilyRole.cpp
    src/FaillockSettings.h src/FaillockSettings.cpp
)
target_include_directories(cairn-accounts PUBLIC src)
target_link_libraries(cairn-accounts PUBLIC Qt6::Core)
set_target_properties(cairn-accounts PROPERTIES POSITION_INDEPENDENT_CODE ON)

qt_add_executable(tst_familyrole tests/tst_familyrole.cpp)
target_link_libraries(tst_familyrole PRIVATE cairn-accounts Qt6::Test)
add_test(NAME familyrole COMMAND tst_familyrole)

qt_add_executable(tst_account tests/tst_account.cpp)
target_link_libraries(tst_account PRIVATE cairn-accounts Qt6::Test)
add_test(NAME account COMMAND tst_account)

qt_add_executable(tst_faillocksettings tests/tst_faillocksettings.cpp)
target_link_libraries(tst_faillocksettings PRIVATE cairn-accounts Qt6::Test)
target_compile_definitions(tst_faillocksettings PRIVATE
    CAIRN_FAILLOCK_CONF="${PROJECT_SOURCE_DIR}/session/security/faillock.conf")
add_test(NAME faillocksettings COMMAND tst_faillocksettings)
