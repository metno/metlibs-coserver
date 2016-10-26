#
# configure.ac for coserver
#

AC_PREREQ(2.59)
# version number must be in sync with src/coserverVersion.h
AC_INIT([coserver], [2.3.0], [diana@met.no])
AM_INIT_AUTOMAKE([foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_SRCDIR([src/miMessage.h])
AC_CONFIG_MACRO_DIR([m4])

METNO_SET_VERSION_INFO(2.0.0)
METNO_PROG_PKG_CONFIG
METNO_REQUIRE_BOOST

AC_PROG_LIBTOOL
AC_SUBST([LIBTOOL_DEPS]) 

AC_PROG_CXX
AC_LANG_PUSH(C++)
AC_CXX_HAVE_STD
AC_CXX_HAVE_STL
AC_LANG_POP([C++])

METNO_ENABLE_CONDITIONAL([SHARED_LIBRARY], [shared])

METNO_REQUIRE_PKG([MILOGGER],[miLogger])
METNO_REQUIRE_PKG([QUTILITIES],[qUtilities])
METNO_REQUIRE_QT4([Core Gui Network])

AC_CONFIG_FILES([
	coserver.pc
	Makefile
	src/Makefile
])

AC_OUTPUT
