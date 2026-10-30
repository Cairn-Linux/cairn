# SPDX-License-Identifier: Apache-2.0
# Where `cmake --install` puts Cairn, and how a file from the repository gets
# there (ADR-0038). The image builds with CMAKE_INSTALL_PREFIX=/usr, so files
# land where an RPM would put them. Every location is a cache variable, so a
# packager can move one without editing this file.
include(GNUInstallDirs)

set(CAIRN_LIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}/cairn" CACHE STRING
    "Cairn's own programs, which no one runs by name")
set(CAIRN_LIBDIR "${CMAKE_INSTALL_LIBDIR}/cairn" CACHE STRING
    "Cairn's private shared libraries")
# Private, not Qt's own QML directory: only the login screen imports these
# modules, through the QML_IMPORT_PATH its SDDM configuration sets.
set(CAIRN_QMLDIR "${CMAKE_INSTALL_LIBDIR}/cairn/qml" CACHE STRING
    "The QML modules the login screen imports")
set(CAIRN_DATADIR "${CMAKE_INSTALL_DATADIR}/cairn" CACHE STRING
    "Cairn's data: the kiosk configuration, the manifest, the session entry")
set(CAIRN_SDDM_THEMESDIR "${CMAKE_INSTALL_DATADIR}/sddm/themes" CACHE STRING
    "Where SDDM looks for login screen themes")
set(CAIRN_SDDM_CONFDIR "lib/sddm/sddm.conf.d" CACHE STRING
    "SDDM's directory for packaged settings")
set(CAIRN_LOGIND_CONFDIR "lib/systemd/logind.conf.d" CACHE STRING
    "logind's directory for packaged settings")
set(CAIRN_POLKITDIR "${CMAKE_INSTALL_DATADIR}/polkit-1" CACHE STRING
    "polkit's directory for packaged rules and actions")

# The same places as absolute paths, for the files that name them.
foreach(dir LIBEXECDIR LIBDIR QMLDIR DATADIR SDDM_THEMESDIR)
    cmake_path(ABSOLUTE_PATH CAIRN_${dir} BASE_DIRECTORY "${CMAKE_INSTALL_PREFIX}"
        OUTPUT_VARIABLE CAIRN_FULL_${dir})
endforeach()

# Provisioning installs the same files under /usr/local (provision/README.md),
# and the files say so. Each of its places, longest first, and where this
# build puts the same thing.
set(cairn_provisioned_places
    "/usr/local/libexec/cairn" "${CAIRN_FULL_LIBEXECDIR}"
    "/usr/local/lib64/cairn/qml" "${CAIRN_FULL_QMLDIR}"
    "/usr/local/lib64/cairn" "${CAIRN_FULL_LIBDIR}"
    "/usr/local/share/cairn/sddm/themes" "${CAIRN_FULL_SDDM_THEMESDIR}"
    "/usr/local/share/cairn" "${CAIRN_FULL_DATADIR}"
    "/usr/local/bin" "${CMAKE_INSTALL_FULL_BINDIR}"
)

# Installs one file from the repository, with provisioning's paths in it
# changed to this build's. A link is followed, so the file itself is
# installed. MODE is 644, 755 or 750.
#
#   cairn_install_file(SOURCE path DESTINATION dir MODE 644 [RENAME name])
#
# DESTINATION is relative to the prefix, or absolute for /etc.
function(cairn_install_file)
    cmake_parse_arguments(PARSE_ARGV 0 arg "" "SOURCE;DESTINATION;MODE;RENAME" "")
    cmake_path(ABSOLUTE_PATH arg_SOURCE BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE source)
    cmake_path(GET source FILENAME name)
    if(arg_RENAME)
        set(name "${arg_RENAME}")
    endif()

    file(READ "${source}" text)
    set(places ${cairn_provisioned_places})
    while(places)
        list(POP_FRONT places from to)
        string(REPLACE "${from}" "${to}" text "${text}")
    endwhile()
    if(NOT CMAKE_INSTALL_PREFIX STREQUAL "/usr/local" AND text MATCHES "/usr/local")
        message(FATAL_ERROR "${source} names a place under /usr/local that "
            "cmake/CairnInstall.cmake does not know. Add it to cairn_provisioned_places.")
    endif()

    # Written only when it changes, so an install after a second configure
    # reports the file up to date.
    set(staged "${CMAKE_BINARY_DIR}/install-staging/${arg_DESTINATION}/${name}")
    set(old "")
    if(EXISTS "${staged}")
        file(READ "${staged}" old)
    endif()
    if(NOT old STREQUAL text)
        file(WRITE "${staged}" "${text}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")

    cairn_permissions(${arg_MODE} permissions)
    install(FILES "${staged}" DESTINATION "${arg_DESTINATION}" PERMISSIONS ${permissions})
endfunction()

# The permissions install() wants for a mode written the usual way.
function(cairn_permissions mode result)
    if(mode STREQUAL "644")
        set(permissions OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    elseif(mode STREQUAL "755")
        set(permissions OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE)
    elseif(mode STREQUAL "750")
        set(permissions OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE)
    else()
        message(FATAL_ERROR "cairn_permissions does not know mode ${mode}")
    endif()
    set(${result} ${permissions} PARENT_SCOPE)
endfunction()
