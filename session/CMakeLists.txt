# SPDX-License-Identifier: Apache-2.0
# Nothing to build here yet; the kiosk config is checked with the other tests.

# Where the image puts each file (ADR-0038). Programs a person or the
# session runs by name go in bin; the rest is data, or packaged settings in
# the folder each program reads them from. /etc holds only PAM's files,
# beside the ones Fedora ships there, and faillock's, read from nowhere else.
foreach(program cairn-session cairn-give-up cairn-log-out)
    cairn_install_file(SOURCE bin/${program} DESTINATION "${CMAKE_INSTALL_BINDIR}" MODE 755)
endforeach()
cairn_install_file(SOURCE bin/cairn-steam-visit DESTINATION "${CAIRN_LIBEXECDIR}" MODE 755)
cairn_install_file(SOURCE labwc/rc.xml DESTINATION "${CAIRN_DATADIR}/labwc" MODE 644)
cairn_install_file(SOURCE labwc/environment DESTINATION "${CAIRN_DATADIR}/labwc" MODE 644)
# Only Cairn's own session folder, which SDDM's SessionDir names (ADR-0012).
cairn_install_file(SOURCE sessions/cairn.desktop DESTINATION "${CAIRN_DATADIR}/sessions" MODE 644)
cairn_install_file(SOURCE polkit/rules.d/10-cairn-levels.rules DESTINATION "${CAIRN_POLKITDIR}/rules.d"
    MODE 644)
cairn_install_file(SOURCE polkit/actions/com.cairnlinux.steam-visit.policy
    DESTINATION "${CAIRN_POLKITDIR}/actions" MODE 644)
cairn_install_file(SOURCE logind.conf.d/10-cairn-power.conf DESTINATION "${CAIRN_LOGIND_CONFDIR}" MODE 644)
# Fedora's plasma-wayland.conf sits in the same folder and sets
# GreeterEnvironment too; SDDM reads the folder in name order, so Cairn's
# file sorts after it.
cairn_install_file(SOURCE sddm/sddm.conf.d/10-cairn.conf DESTINATION "${CAIRN_SDDM_CONFDIR}"
    RENAME zz-cairn.conf MODE 644)
# This one has to beat a file Plasma's setup writes to /etc/sddm.conf.d,
# which SDDM reads after its packaged folder.
cairn_install_file(SOURCE sddm/sddm.conf.d/zz-cairn-no-autologin.conf
    DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/sddm.conf.d" MODE 644)
cairn_install_file(SOURCE sddm/pam.d/sddm DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/pam.d" MODE 644)
cairn_install_file(SOURCE pam.d/cairn-steam-visit DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/pam.d"
    MODE 644)
cairn_install_file(SOURCE pam.d/login DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/pam.d" MODE 644)
cairn_install_file(SOURCE security/faillock.conf DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/security"
    MODE 644)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME labwc_config
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_labwc_config.py"
)
add_test(NAME cairn_session
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cairn_session.py"
)
add_test(NAME pam_sddm
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pam_sddm.py"
)
add_test(NAME polkit_rules
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_polkit_rules.py"
)
add_test(NAME give_up
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_give_up.py"
)
add_test(NAME log_out
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_log_out.py"
)
add_test(NAME steam_visit
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_steam_visit.py"
)
add_test(NAME pam_steam_visit
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pam_steam_visit.py"
)
