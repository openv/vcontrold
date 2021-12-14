# Copyright 2012 Johannes Zarl <johannes@zarl-zierl.at>
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# This file is under the public domain and can be reused without restrictions.

if (NOT DEFINED BASE_DIR)
    message (FATAL_ERROR "UpdateVersion.cmake: BASE_DIR not set. "
                         "Please supply base working directory!")
endif()

# Version header locations
set(VERSION_H_IN        ${BASE_DIR}/src/version.h.in)
set(VERSION_H_CURRENT   ${CMAKE_BINARY_DIR}/version.h.current)
set(VERSION_H_GENERATED ${CMAKE_BINARY_DIR}/version.h)

# git or tarball?
if (EXISTS ${BASE_DIR}/.git)
    # --> git:
    include (${CMAKE_CURRENT_LIST_DIR}/GitDescription.cmake)
    git_get_description (VERSION GIT_ARGS --dirty)
    if (NOT VERSION)
        set (VERSION "unknown")
    else()
        # Remove a trailing "v" from the version.
        string(REGEX REPLACE "^v" "" VERSION ${VERSION})
    endif()

    message (STATUS "Updating version information to ${VERSION} ...")

else()
    # --> tarball
    if (NOT EXISTS ${BASE_DIR}/src/version.h)
        message (WARNING "The generated file \"version.h\" does not exist!")
        message (WARNING "Either, something went wrong when releasing this tarball, or this is "
                         "some GitHub snapshot.")
        get_filename_component(BASE_DIR_L "${BASE_DIR}" NAME)
        string(REGEX REPLACE "[^-]+-([0-9]+)\\.([0-9]+)\\.([0-9]+)(.*)" "\\1.\\2.\\3\\4"
                             VERSION "${BASE_DIR_L}")
        if ("${VERSION}" STREQUAL "${BASE_DIR_L}")
            message(WARNING "Could not derive version from name of project directory, set version "
                            "as \"unknown\".")
            set (VERSION "unknown")
        else()
            message(WARNING "Version guessed from project directory name as \"${VERSION}\".")
        endif()
        message (WARNING "Generating a dummy version.h.")
    endif()

endif()

# write version info to a temporary file
configure_file(${VERSION_H_IN} ${VERSION_H_CURRENT})
# update info if changed
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${VERSION_H_CURRENT} ${VERSION_H_GENERATED})
# make sure info doesn't get stale
file(REMOVE ${VERSION_H_CURRENT})
