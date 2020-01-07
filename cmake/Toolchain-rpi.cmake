# CMAKE_TOOLCHAIN_FILE for cross-compiling for RPi et al.

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_VERSION 1)

if(DEFINED ENV{CHROOT_DIR})
    SET(CHROOT_DIR $ENV{CHROOT_DIR})
else(DEFINED ENV{CHROOT_DIR})
    SET(CHROOT_DIR /mnt/raspbian_lite)
endif(DEFINED ENV{CHROOT_DIR})

SET(CMAKE_C_COMPILER   arm-linux-gnueabihf-gcc)
SET(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

SET(CMAKE_SYSROOT ${CHROOT_DIR})
SET(CMAKE_FIND_ROOT_PATH ${CHROOT_DIR})

# don't search for programs on CHROOT_DIR, use those on build host
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search libraries and headers on CHROOT_DIR
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

SET(FLAGS "-Wl,-rpath-link,${CHROOT_DIR}/lib/arm-linux-gnueabihf -Wl,-rpath-link,${CHROOT_DIR}/usr/lib/arm-linux-gnueabihf -Wl,-rpath-link,${CHROOT_DIR}/usr/local/lib,-unresolved-symbols=ignore-in-shared-libs")

UNSET(CMAKE_C_FLAGS CACHE)
UNSET(CMAKE_CXX_FLAGS CACHE)
SET(CMAKE_C_FLAGS ${FLAGS} CACHE STRING "" FORCE)
SET(CMAKE_CXX_FLAGS ${FLAGS} CACHE STRING "" FORCE)
