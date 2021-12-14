# Copyright 2012 Johannes Zarl-Zierl <johannes@zarl-zierl.at>
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
# This file is under the public domain and can be reused without restrictions.

include (CMakeParseArguments)

function (git_get_description DESCVAR)
    cmake_parse_arguments (_GGD "SEND_ERROR" "GIT_ARGS" "" "${ARGN}")
    if (SEND_ERROR)
        set (_severity SEND_ERROR)
    else()
        set (_severity WARNING)
    endif()

    find_package (Git QUIET)

    if (NOT GIT_FOUND)
        message (${severity} "git_get_description: could not find package git!")
        set (${DESCVAR} "-NOTFOUND" PARENT_SCOPE)
        return()
    endif()

    execute_process (COMMAND "${GIT_EXECUTABLE}" describe ${_GGD_GIT_ARGS}
                     WORKING_DIRECTORY "${BASE_DIR}"
                     RESULT_VARIABLE _gitresult
                     OUTPUT_VARIABLE _gitdesc
                     ERROR_VARIABLE  _giterror
                     OUTPUT_STRIP_TRAILING_WHITESPACE )

    if (NOT _gitresult EQUAL 0)
        message (${_severity} "git_get_description: error during execution of git describe!")
        message ( ${_severity} "Error was: ${_giterror}" )
        set (${DESCVAR} "-NOTFOUND" PARENT_SCOPE)
    else()
        set (${DESCVAR} "${_gitdesc}" PARENT_SCOPE)
    endif()

endfunction()
