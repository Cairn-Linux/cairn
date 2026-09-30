# SPDX-License-Identifier: Apache-2.0
# Nothing to build; the provisioning script's guarded steps run with
# stand-ins for the system's commands.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME provision
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_provision.py"
)
