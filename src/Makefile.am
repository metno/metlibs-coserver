# $Id: Makefile.am 4334 2013-12-19 14:42:10Z juergens $

AM_CPPFLAGS = -DPKGCONFDIR='"$(sysconfdir)/@PACKAGE_NAME@"'

lib_LTLIBRARIES = libcoserver.la

cc_sources= \
	QLetterCommands.cc \
	miMessage.cc \
	miMessageIO.cc

qt_sources = \
	ClientSelection.cc \
	ClientButton.cc \
	CoClient.cc 

img_sources=\
	conn.xpm \
	disconn.xpm \
	unconn.xpm


# Generated Qt source code 
moc_sources = $(qt_sources:.cc=.moc.cc) 
%.moc.cc: %.h
	$(MOC4) -o $@ $<


nodist_libcoserver_la_SOURCES = $(moc_sources)

libcoserver_la_SOURCES = $(cc_sources) $(qt_sources)

pkg_includedir = ${includedir}/@PACKAGE_NAME@

pkg_include_HEADERS = \
	$(cc_sources:.cc=.h) \
	$(qt_sources:.cc=.h) \
	coserverVersion.h


libcoserver_la_CPPFLAGS = ${MILOGGER_CPPFLAGS} ${QUTILITIES_CPPFLAGS} ${QT4_CPPFLAGS} ${AM_CPPFLAGS}
libcoserver_la_LDFLAGS  = ${VERSION_INFO}


if ENABLE_SHARED_LIBRARY

libcoserver_la_LDFLAGS += ${MILOGGER_LDFLAGS} ${QUTILITIES_LDFLAGS} ${QT4_LDFLAGS}
libcoserver_la_LIBADD   = ${MILOGGER_LIBS}    ${QUTILITIES_LIBS}    ${QT4_LIBS}

endif


CLEANFILES = $(moc_sources)

INSTALL = install -p
