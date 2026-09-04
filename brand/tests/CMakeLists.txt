# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.28)

project(CairnBrandTests LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Quick QuickTest)

qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../qml/Cairn/Brand"
    "${CMAKE_CURRENT_BINARY_DIR}/Cairn/Brand"
)

qt_add_executable(cairn_brand_tokens_test
    tst_tokens_main.cpp
)

target_link_libraries(cairn_brand_tokens_test PRIVATE
    Qt6::Quick
    Qt6::QuickTest
    CairnBrand
)

target_compile_options(cairn_brand_tokens_test PRIVATE
    -Wall
    -Wextra
    -Werror
)

add_test(
    NAME cairn_brand_tokens
    COMMAND $<TARGET_FILE:cairn_brand_tokens_test>
        -input "${CMAKE_CURRENT_SOURCE_DIR}/tst_tokens.qml"
        -import "${CMAKE_CURRENT_BINARY_DIR}"
)

set_tests_properties(cairn_brand_tokens PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
