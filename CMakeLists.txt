cmake_minimum_required(VERSION 3.0.0)
project(vcontrold C)

IF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
# CMAKE_INSTALL_PREFIX defaults to /usr/local, but it should go to /usr
set(CMAKE_INSTALL_PREFIX /usr)
ENDIF(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

find_package(LibXml2 REQUIRED)

include_directories(
    ${LIBXML2_INCLUDE_DIR}
)

set(vcontrold_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xmlconfig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/semaphore.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/framer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/unit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vcontrold.c
)

set(vclient_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vclient.c
)

set(vsim_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vsim.c
)

add_executable(vcontrold ${vcontrold_SRCS})
target_link_libraries(vcontrold ${LIBXML2_LIBRARIES})

add_executable(vclient ${vclient_SRCS})

add_executable(vsim ${vsim_SRCS})

install(TARGETS vcontrold DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS vclient DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS vsim DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

file(GLOB MANPAGES ${CMAKE_CURRENT_SOURCE_DIR}/doc/man/*.1)
install(FILES ${MANPAGES} DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/man1)
