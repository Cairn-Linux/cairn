# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.28)

project(cairn LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Werror)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

find_package(Qt6 REQUIRED COMPONENTS Quick QuickTest Test)
qt_standard_project_setup()
qt_policy(SET QTP0001 NEW)
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

enable_testing()
add_subdirectory(brand/qml/Cairn/Brand)
add_subdirectory(launcher)
add_subdirectory(session)
add_subdirectory(shell)
