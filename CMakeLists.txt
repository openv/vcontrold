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
    src/io.c
    src/common.c
    src/xmlconfig.c
    src/parser.c
    src/socket.c
    src/semaphore.c
    src/framer.c
    src/unit.c
    src/arithmetic.c
    src/vcontrold.c
)

set(vclient_SRCS
    src/common.c
    src/socket.c
    src/io.c
    src/client.c
    src/vclient.c
)

set(vsim_SRCS
    src/socket.c
    src/vsim.c
)

add_executable(vcontrold ${vcontrold_SRCS})
target_link_libraries(vcontrold ${LIBXML2_LIBRARIES})

add_executable(vclient ${vclient_SRCS})

add_executable(vsim ${vsim_SRCS})

install(TARGETS vcontrold DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS vclient DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS vsim DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
