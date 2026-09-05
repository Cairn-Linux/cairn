# SPDX-License-Identifier: Apache-2.0
# Nothing to build here yet; the kiosk config is checked with the other tests.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME labwc_config
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_labwc_config.py"
)
