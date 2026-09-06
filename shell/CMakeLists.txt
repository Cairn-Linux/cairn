# SPDX-License-Identifier: Apache-2.0
find_package(Qt6 REQUIRED COMPONENTS Core LinguistTools Test)

# The interpreter and what it sees, with no GUI dependency, so the trust
# boundary (what the shell refuses) is tested on its own.
qt_add_library(footpath-core STATIC
    src/Completion.h src/Completion.cpp
    src/DidYouMean.h src/DidYouMean.cpp
    src/Hints.h src/Hints.cpp
    src/HomeLayout.h src/HomeLayout.cpp
    src/Interpreter.h src/Interpreter.cpp
    src/Reply.h
    src/World.h src/World.cpp
)
target_include_directories(footpath-core PUBLIC src)
target_link_libraries(footpath-core PUBLIC Qt6::Core)

qt_add_lupdate(
    SOURCE_TARGETS footpath-core
    TS_FILES i18n/shell_en.ts
    LUPDATE_TARGET shell_lupdate
)

foreach(suite world homelayout didyoumean interpreter)
    qt_add_executable(tst_shell_${suite} tests/tst_${suite}.cpp)
    target_link_libraries(tst_shell_${suite} PRIVATE Qt6::Test footpath-core)
    add_test(NAME shell_${suite} COMMAND tst_shell_${suite})
endforeach()
