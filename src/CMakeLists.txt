
SET(CMAKE_AUTOMOC TRUE)
SET(CMAKE_INCLUDE_CURRENT_DIR ON)

INCLUDE_DIRECTORIES(${PC_METLIBS_INCLUDE_DIRS})
ADD_DEFINITIONS(-DQT_NO_KEYWORDS -W -Wall ${PC_METLIBS_CFLAGS_OTHER})
LINK_DIRECTORIES(${PC_METLIBS_LIBRARY_DIRS})

SET (COSERVER_CONF_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}/coserver" CACHE STRING "coserver configuration directory, e.g. for client.ini")
ADD_DEFINITIONS(-DPKGCONFDIR="${COSERVER_CONF_DIR}")

########################################################################

SET(coserver_SOURCES
  QLetterCommands.cc
  miMessage.cc
  miMessageIO.cc
  ClientSelection.cc
  ClientButton.cc
  CoClient.cc
)

METNO_HEADERS (coserver_HEADERS coserver_SOURCES ".cc" ".h")
LIST(APPEND coserver_HEADERS
  coserverVersion.h
)

LIST(APPEND coserver_SOURCES
  conn.xpm
  disconn.xpm
  unconn.xpm
)

########################################################################

ADD_LIBRARY(coserver SHARED
  ${coserver_SOURCES}
)

SET_TARGET_PROPERTIES(coserver PROPERTIES
  VERSION "${COSERVER_PVERSION_FULL}"
  SOVERSION "${COSERVER_VERSION_MAJOR}"
  OUTPUT_NAME "${lib_name}"
)

TARGET_LINK_LIBRARIES(coserver
  ${QT_LIBRARIES}
  ${PC_METLIBS_LIBRARIES}
)

INSTALL(TARGETS coserver
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

INSTALL(FILES ${coserver_HEADERS}
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/metlibs/coserver
)
