# Generate classic *roff manual pages from ReStructuredText.
# This requires Python's Docutils (http://docutils.sourceforge.net/)

cmake_minimum_required(VERSION 3.2.0)

find_program(RST2MAN_EXE NAMES rst2man rst2man.py)
set(RST2MAN_OPTS)

set(MANUALS
    vcontrold
    vclient
    vsim
)

if(RST2MAN_EXE)
    foreach(MANUAL IN LISTS MANUALS)
        set(MANPAGE_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${MANUAL}.1.gz)
        set(MANPAGE_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/${MANUAL}.rst)
        add_custom_command(OUTPUT ${MANPAGE_OUTPUT}
            COMMAND ${RST2MAN_EXE} ${RST2MAN_OPTS} ${MANPAGE_SOURCE} | gzip > ${MANPAGE_OUTPUT}
            DEPENDS ${MANPAGE_SOURCE}
            BYPRODUCTS ${MANPAGE_OUTPUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Converting ${MANUAL}.rst from ReStructuredText into manpage ${MANUAL}.1.gz"
            VERBATIM
        )
    list(APPEND MANPAGES ${MANPAGE_OUTPUT})
    endforeach()

    add_custom_target(man ALL DEPENDS ${MANPAGES})
    install(FILES ${MANPAGES} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
else(RST2MAN_EXE)
    message("WARNING: rst2html not found. The manpages won't be built.")
    message("         Install Python's Docutils (http://docutils.sourceforge.net/)")
    message("         If you want to build the manpages.")
endif(RST2MAN_EXE)
