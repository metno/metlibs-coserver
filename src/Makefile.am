# $Id: Makefile.am 4334 2013-12-19 14:42:10Z juergens $

pkgname = coserver

lib_LTLIBRARIES = libcoserver.la

cc_sources= \
	QLetterCommands.cc \
	miMessage.cc

qt_sources = \
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

pkg_includedir = ${includedir}/coserver

pkg_include_HEADERS = \
	$(cc_sources:.cc=.h)\
	$(qt_sources:.cc=.h)\
	$(img_sources)


libcoserver_la_CPPFLAGS = ${MILOGGER_CPPFLAGS} ${QT4_CPPFLAGS}
libcoserver_la_LDFLAGS  = ${VERSION_INFO}


if ENABLE_SHARED_LIBRARY

libcoserver_la_LDFLAGS += ${MILOGGER_LDFLAGS} ${QT4_LDFLAGS}
libcoserver_la_LIBADD   = ${MILOGGER_LIBS}    ${QT4_LIBS}

endif


CLEANFILES = $(moc_sources)

INSTALL = install -p
