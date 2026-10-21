# SPDX-License-Identifier: Apache-2.0
# The install tree the image is made from (ADR-0038): every file, its place
# and its mode. The test installs this build into a temporary folder, so it
# needs every target built first, as ctest always has.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME install_tree
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_install_tree.py"
        "${CMAKE_COMMAND}" "${CMAKE_BINARY_DIR}" "${CMAKE_INSTALL_PREFIX}"
)
# A build for provisioning's /usr/local has no image layout to check.
set_tests_properties(install_tree PROPERTIES SKIP_RETURN_CODE 77)
