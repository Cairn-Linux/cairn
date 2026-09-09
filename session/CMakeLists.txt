# SPDX-License-Identifier: Apache-2.0
# Nothing to build here yet; the kiosk config is checked with the other tests.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME labwc_config
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_labwc_config.py"
)
add_test(NAME cairn_session
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cairn_session.py"
)
add_test(NAME polkit_rules
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_polkit_rules.py"
)
add_test(NAME give_up
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_give_up.py"
)
