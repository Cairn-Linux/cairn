# SPDX-License-Identifier: Apache-2.0
find_package(Qt6 REQUIRED COMPONENTS Core LinguistTools Test)

# The interpreter and what it sees, with no GUI dependency, so the trust
# boundary (what the shell refuses) is tested on its own.
qt_add_library(cairn-shell-core STATIC
    src/AppTree.h src/AppTree.cpp
    src/Completion.h src/Completion.cpp
    src/DidYouMean.h src/DidYouMean.cpp
    src/Hints.h src/Hints.cpp
    src/Interpreter.h src/Interpreter.cpp
    src/Reply.h
)
target_include_directories(cairn-shell-core PUBLIC src)
target_link_libraries(cairn-shell-core PUBLIC Qt6::Core)

qt_add_lupdate(
    SOURCE_TARGETS cairn-shell-core
    TS_FILES i18n/shell_en.ts
    LUPDATE_TARGET shell_lupdate
)

foreach(suite apptree didyoumean interpreter)
    qt_add_executable(tst_shell_${suite} tests/tst_${suite}.cpp)
    target_link_libraries(tst_shell_${suite} PRIVATE Qt6::Test cairn-shell-core)
    add_test(NAME shell_${suite} COMMAND tst_shell_${suite})
endforeach()
