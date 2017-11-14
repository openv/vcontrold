# generate classic *roff manual pages from markdown
# this requires md2man (https://github.com/sunaku/md2man)

cmake_minimum_required(VERSION 3.2.0)

find_program(MD2MAN_EXE NAMES md2man-roff)
set(MD2MAN_OPTS )

#FILE(GLOB MANPAGES *.md )
set(MANUALS
  vcontrold
  vclient
  vsim
)

foreach(m IN LISTS MANUALS)
  set(mf ${CMAKE_CURRENT_BINARY_DIR}/${m}.1.gz)
  set(ms ${CMAKE_CURRENT_SOURCE_DIR}/${m}.md)
  add_custom_command(OUTPUT ${mf}
    COMMAND ${MD2MAN_EXE} ${MD2MAN_OPTS} ${ms} | gzip > ${mf}
    DEPENDS ${ms}
	BYPRODUCTS ${mf}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Converting ${m}.md from markdown into manpage ${m}.1.gz"
    VERBATIM)
list(APPEND MANPAGES ${mf})
endforeach()

add_custom_target(man ALL DEPENDS ${MANPAGES})
install(FILES ${MANPAGES} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
