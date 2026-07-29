# SPDX-FileCopyrightText: 2017-2026 Tobias Leupold <tl@stonemx.de>
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22.0)
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
set(vcontrold_core_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xmlconfig.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/semaphore.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/framer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/unit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic.c
)

set(vclient_core_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client.c
)

set(vsim_core_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
)


find_package(Threads)
set(LIBS
    ${LIBS}
    ${LIBXML2_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

add_library(vcontrold_core STATIC ${vcontrold_core_SRCS})
target_link_libraries(vcontrold_core PUBLIC ${LIBS} m)
target_include_directories(vcontrold_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${LIBXML2_INCLUDE_DIR}
)
target_compile_definitions(vcontrold_core PRIVATE _XOPEN_SOURCE=700)

add_executable(vcontrold ${CMAKE_CURRENT_SOURCE_DIR}/src/vcontrold.c)
target_link_libraries(vcontrold PRIVATE vcontrold_core)
target_include_directories(vcontrold PRIVATE ${CMAKE_BINARY_DIR})
target_compile_definitions(vcontrold PRIVATE _XOPEN_SOURCE=700)
add_dependencies(vcontrold UpdateVersion)

if(VCLIENT)
    add_library(vclient_core STATIC ${vclient_core_SRCS})
    target_link_libraries(vclient_core PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})
    target_include_directories(vclient_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(vclient_core PRIVATE _XOPEN_SOURCE=700)

    add_executable(vclient ${CMAKE_CURRENT_SOURCE_DIR}/src/vclient.c)
    target_link_libraries(vclient PRIVATE vclient_core)
    target_include_directories(vclient PRIVATE ${CMAKE_BINARY_DIR})
    target_compile_definitions(vclient PRIVATE _XOPEN_SOURCE=700)
    add_dependencies(vclient UpdateVersion)
endif()

if(VSIM)
    add_library(vsim_core STATIC ${vsim_core_SRCS})
    target_include_directories(vsim_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(vsim_core PRIVATE _XOPEN_SOURCE=700)

    add_executable(vsim ${CMAKE_CURRENT_SOURCE_DIR}/src/vsim.c)
    target_link_libraries(vsim PRIVATE vsim_core)
    target_include_directories(vsim PRIVATE ${CMAKE_BINARY_DIR})
    target_compile_definitions(vsim PRIVATE _XOPEN_SOURCE=700)
    add_dependencies(vsim UpdateVersion)
endif()

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
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

    add_executable(test_io
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_io.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
    )
    target_include_directories(test_io PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(test_io PRIVATE _XOPEN_SOURCE=700)
    add_test(NAME io COMMAND test_io)

    add_executable(test_semaphore
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_semaphore.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/semaphore.c
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
    )
    target_include_directories(test_semaphore PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(test_semaphore PRIVATE _XOPEN_SOURCE=700)
    add_test(NAME semaphore COMMAND test_semaphore)

    if(CMAKE_SYSTEM_NAME MATCHES "Linux|CYGWIN")
        add_executable(test_alloc_failures
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_alloc_failures.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/client.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/xmlconfig.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/unit.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/arithmetic.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/common.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/io.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/framer.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/socket.c
        )
        target_include_directories(test_alloc_failures PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src ${LIBXML2_INCLUDE_DIR}
        )
        target_compile_definitions(test_alloc_failures PRIVATE
            _XOPEN_SOURCE=700
            VCONTROLD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_link_libraries(test_alloc_failures PRIVATE ${LIBS} m)
        target_link_options(test_alloc_failures PRIVATE
            -Wl,--wrap=calloc -Wl,--wrap=realloc
        )
        add_test(NAME alloc_failures COMMAND test_alloc_failures)
    endif()

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

    add_test(
        NAME xml_profiles
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_xml_profiles.py
    )

    if(VCLIENT)
        add_test(
            NAME vclient_integration
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vclient_integration.py
                $<TARGET_FILE:vclient>
        )
        set_tests_properties(vclient_integration PROPERTIES TIMEOUT 30)
    endif()
    if(VSIM)
        add_test(
            NAME vsim_integration
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vsim_integration.py
                $<TARGET_FILE:vsim>
        )
        set_tests_properties(vsim_integration PROPERTIES TIMEOUT 20)

        add_test(
            NAME vcontrold_integration
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vcontrold_integration.py
                $<TARGET_FILE:vcontrold>
                $<TARGET_FILE:vsim>
                ${CMAKE_CURRENT_SOURCE_DIR}/xml/300
        )
        set_tests_properties(vcontrold_integration PROPERTIES TIMEOUT 40)
    endif()
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
