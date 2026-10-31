# SPDX-License-Identifier: Apache-2.0
# Nothing to build; the provisioning script's guarded steps run with
# stand-ins for the system's commands.

# The copy step, the picture step, the tools step and kidscan, whose rules
# they share, are run with python3 -I, so none is executable (ADR-0033,
# ADR-0038, ADR-0046).
cairn_install_file(SOURCE cairn-copy-games.py DESTINATION "${CAIRN_LIBEXECDIR}" RENAME cairn-copy-games
    MODE 644)
cairn_install_file(SOURCE cairn-game-pictures.py DESTINATION "${CAIRN_LIBEXECDIR}"
    RENAME cairn-game-pictures MODE 644)
cairn_install_file(SOURCE cairn-share-tools.py DESTINATION "${CAIRN_LIBEXECDIR}"
    RENAME cairn-share-tools MODE 644)
cairn_install_file(SOURCE ../tools/kidscan/kidscan.py DESTINATION "${CAIRN_LIBEXECDIR}" MODE 644)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME provision
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_provision.py"
)
# The family's setup on a computer that runs the image, and the renumbering
# of cairn-guardian before a computer set up by hand moves to it (ADR-0038).
add_test(NAME family_setup
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_family_setup.py"
)
# The copy of each shared game into a child's library (ADR-0033). Its copies
# need a filesystem that can reflink; the test looks for one in the
# temporary folder, then under build/.
add_test(NAME copy_games
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_copy_games.py"
)
# The copy of each shared game's picture from the Guardian's Steam
# (ADR-0033, ADR-0035).
add_test(NAME game_pictures
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_game_pictures.py"
)
# The copy of Steam's tools from a Guardian's Steam into the tools folder
# (ADR-0046). Its copies need reflink, found as the copy step's test finds
# it.
add_test(NAME share_tools
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_share_tools.py"
)
