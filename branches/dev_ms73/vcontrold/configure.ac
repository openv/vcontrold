##                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(vcontrol, 0.98, mschroether@gmx.de)

AM_INIT_AUTOMAKE(no-dependencies)

AC_CONFIG_SRCDIR([vcontrold.c])
#AC_CONFIG_HEADER([config.h])

# Checks for programs.
AC_PROG_CC

# xml2
AC_ARG_WITH(xml2-include-dir,
	AC_HELP_STRING(
		[--with-xml2-include-dir],
		[XML2 include directory]),
	[xml2_include_dir=$withval])
AC_ARG_WITH(xml2-lib-dir,
	AC_HELP_STRING(
		[--with-xml2-lib-dir],
		[XML2 library directory]),
	[xml2_lib_dir=$withval])


# Checks for libraries.

## Determine XML2 library path
# try and find libs without any additional libraries
AC_CHECK_LIB(xml2, xmlParseFile,
	XML2_LIBS=" ",
	XML2_LIBS=
)

# hunt through several directories trying to find the libraries
if test "x$XML2_LIBS" = "x"; then
	old_LDFLAGS="$LDFLAGS"
	for try in $xml2_lib_dir /usr/lib64 /usr/lib /usr/local/lib ; do
		LDFLAGS="$old_LDFLAGS -L$try"
		unset ac_cv_lib_xml2_xmlParseFile
		AC_CHECK_LIB(xml2, xmlParseFile,
			XML2_LIBS="-L$try",
			XML2_LIBS=
		)
		if test "x$XML2_LIBS" != "x"; then
			break;
		fi
	done
	LDFLAGS="$old_LDFLAGS $XML2_LIBS"
fi

if test "x$XML2_LIBS" = "x"; then
	AC_MSG_ERROR([XML2 libraries not found.  Use --with-xml2-lib-dir=<path>])
fi
# don't forget to add xml2 to the list of libraries
LIBS="-lxml2 $LIBS"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h stdint.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h termios.h unistd.h sys/types.h sys/ipc.h sys/sem.h])

## Determine XML2 include path
AC_MSG_CHECKING(for libxml/xmlmemory.h)

# Can we include headers using system include dirs?
AC_TRY_COMPILE([#include <libxml/xmlmemory.h>], [int a = 1;],
	XML2_INCLUDE=" ",
	XML2_INCLUDE=
)

# hunt through several directories trying to find the includes
if test "x$XML2_INCLUDE" = "x"; then
	old_CPPFLAGS="$CPPFLAGS"
	for i in $xml2_include_dir /usr/include /usr/local/include /usr/include/libxml2 /usr/local/include/libxml2 ; do
		CPPFLAGS="$old_CPPFLAGS -I$i"
		AC_TRY_COMPILE([#include <libxml/xmlmemory.h>], [int a = 1;],
			XML2_INCLUDE="-I$i",
			XML2_INCLUDE=
		)
		if test "x$XML2_INCLUDE" != "x"; then
			break;
		fi
	done
	CPPFLAGS="$old_CPPFLAGS $XML2_INCLUDE"
fi

if test "x$XML2_INCLUDE" = "x"; then
	AC_MSG_RESULT(no) 
	AC_MSG_ERROR([XML2 headers not found.  Use --with-xml2-include-dir=<path>])
fi
AC_MSG_RESULT(yes) 

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT8_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_STRUCT_TM
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([alarm bzero gethostbyname socket strchr strerror strstr])

AC_OUTPUT(Makefile)
