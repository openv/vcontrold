# SPDX-FileCopyrightText: 2017-2022 Tobias Leupold <tl at stonemx dot de>
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.2...3.31)
project(vcontrold C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

include(CTest)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # CMAKE_INSTALL_PREFIX defaults to /usr/local, but it should go to /usr
    set(CMAKE_INSTALL_PREFIX /usr)
endif()

option(MANPAGES "Build man pages via rst2man" ON)
option(VCLIENT "Build the vclient helper program (for communication with vcontrold)" ON)
option(VSIM "Build the vsim helper program (for development and testing purposes)" OFF)

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
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/UpdateVersion.cmake
    COMMENT "Updating version header."
    BYPRODUCTS ${CMAKE_BINARY_DIR}/version.h
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
target_include_directories(vcontrold PRIVATE ${CMAKE_BINARY_DIR} ${LIBXML2_INCLUDE_DIR})
target_compile_definitions(vcontrold PRIVATE _XOPEN_SOURCE=700)
add_dependencies(vcontrold UpdateVersion)

if(VCLIENT)
    add_executable(vclient ${vclient_SRCS})
    target_include_directories(vclient PRIVATE ${CMAKE_BINARY_DIR})
    target_compile_definitions(vclient PRIVATE _XOPEN_SOURCE=700)
    add_dependencies(vclient UpdateVersion)
endif()

if(VSIM)
    add_executable(vsim ${vsim_SRCS})
    target_include_directories(vsim PRIVATE ${CMAKE_BINARY_DIR})
    target_compile_definitions(vsim PRIVATE _XOPEN_SOURCE=700)
    add_dependencies(vsim UpdateVersion)
endif()

if(BUILD_TESTING)
    add_executable(test_common
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
    )
    target_include_directories(test_common PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(test_common PRIVATE _XOPEN_SOURCE=700)
    add_test(NAME common COMMAND test_common)

    add_executable(test_client
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_client.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
    )
    target_include_directories(test_client PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(test_client PRIVATE _XOPEN_SOURCE=700)
    add_test(NAME client COMMAND test_client)

    add_executable(test_parser
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_parser.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/xmlconfig.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/unit.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/framer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
    )
    target_include_directories(test_parser PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${LIBXML2_INCLUDE_DIR}
    )
    target_compile_definitions(test_parser PRIVATE _XOPEN_SOURCE=700)
    target_link_libraries(test_parser ${LIBS} m)
    add_test(NAME parser COMMAND test_parser)
    set_tests_properties(parser PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    add_executable(test_framer
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_framer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/framer.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
    )
    target_include_directories(test_framer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(test_framer PRIVATE
        _XOPEN_SOURCE=700 VCONTROLD_TESTING
    )
    add_test(NAME framer COMMAND test_framer)

    add_executable(test_arithmetic
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_arithmetic.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic.c
    )
    target_include_directories(test_arithmetic PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(test_arithmetic PRIVATE _XOPEN_SOURCE=700)
    target_link_libraries(test_arithmetic m)
    add_test(NAME arithmetic COMMAND test_arithmetic)

    add_executable(test_cycle_time
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cycle_time.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/unit.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/xmlconfig.c
    )
    target_include_directories(test_cycle_time PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src ${LIBXML2_INCLUDE_DIR}
    )
    target_compile_definitions(test_cycle_time PRIVATE _XOPEN_SOURCE=700)
    target_link_libraries(test_cycle_time ${LIBS} m)
    add_test(NAME cycle_time COMMAND test_cycle_time)
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
