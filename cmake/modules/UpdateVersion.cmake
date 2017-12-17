# Copyright 2012 Johannes Zarl <johannes@zarl-zierl.at>
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# This file is under the public domain and can be reused without restrictions.

if (NOT DEFINED BASE_DIR)
    message (FATAL_ERROR "UpdateVersion.cmake: BASE_DIR not set. Please supply base working directory!")
endif()

function (createVersionH version)
    # write version info to a temporary file
    configure_file (${BASE_DIR}/src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h~)
    # update info if changed
    execute_process (COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/version.h~ ${BASE_DIR}/src/version.h)
    # make sure info doesn't get stale
    file (REMOVE ${CMAKE_CURRENT_BINARY_DIR}/version.h~)
endfunction(createVersionH)

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
    createVersionH(${VERSION})

else()
    # --> tarball
    if (NOT EXISTS ${BASE_DIR}/src/version.h)
        message (WARNING "The generated file \"version.h\" does not exist!")
        message (WARNING "Either, something went wrong when releasing this tarball, or this is some GitHub snapshot.")
        message (WARNING "Generating a dummy version.h.")
        set (VERSION "unknown")
        createVersionH(${VERSION})
    endif()
endif()
