cmake_minimum_required(VERSION 3.2.0)
project(vcontrold C)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # CMAKE_INSTALL_PREFIX defaults to /usr/local, but it should go to /usr
    set(CMAKE_INSTALL_PREFIX /usr)
endif()

option(MANPAGES "Build man pages via rst2man" ON)
option(VCLIENT "Build the vclient helper program (for communication with vcontrold)" ON)
option(VSIM "Build the vsim helper program (for development and testing purposes)" OFF)

# Default to -fcommon for GCC 10
if (CMAKE_C_COMPILER_ID MATCHES "GNU" AND CMAKE_C_COMPILER_VERSION VERSION_GREATER 10)
    set (CMAKE_C_FLAGS "-fcommon ${CMAKE_C_FLAGS}")
endif ()

# additional define to compile on macOS Catalina by hmueller01
if (APPLE)
    set (CMAKE_C_FLAGS "-D_DARWIN_C_SOURCE ${CMAKE_C_FLAGS}")
endif (APPLE)

find_package(LibXml2 REQUIRED)

if(MANPAGES)
    find_program(RST2MAN NAMES rst2man rst2man.py)
    if(RST2MAN)
        message(STATUS "Found rst2man: ${RST2MAN}")
    else()
        message(FATAL_ERROR "Could not find a rst2man executable. Either set the \"MANPAGES\" "
                            "option to \"OFF\" (via cmake -DMANPAGES=OFF) or install Python's "
                            "Docutils (cf. http://docutils.sourceforge.net/).")
    endif()
endif()

add_custom_target(
    UpdateVersion ALL
    COMMAND ${CMAKE_COMMAND}
        -DBASE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/UpdateVersion.cmake
    COMMENT "Updating version header."
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/src/version.h
)

add_definitions(-D_XOPEN_SOURCE=700)

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


find_package(Threads)
set(LIBS
    ${LIBS}
    ${LIBXML2_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

add_executable(vcontrold ${vcontrold_SRCS})
target_link_libraries(vcontrold ${LIBS})
add_dependencies(vcontrold UpdateVersion)

if(VCLIENT)
    add_executable(vclient ${vclient_SRCS})
    add_dependencies(vclient UpdateVersion)
endif()

if(VSIM)
    add_executable(vsim ${vsim_SRCS})
    add_dependencies(vsim UpdateVersion)
endif()

if(MANPAGES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/doc/man)
endif()

install(TARGETS vcontrold DESTINATION ${CMAKE_INSTALL_PREFIX}/sbin)
if(VCLIENT)
    install(TARGETS vclient DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
if(VSIM)
    install(TARGETS vsim DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()
