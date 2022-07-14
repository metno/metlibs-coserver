PROJECT(metlibs-coserver)
CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
INCLUDE(MetnoUtils)
INCLUDE(MetnoQtUtils)

METNO_PVERSION_DEFINES(COSERVER "src/coserverVersion.h")

METNO_FIND_QT(Widgets Network)

PKG_CHECK_MODULES(PC_METLIBS REQUIRED
  metlibs-qutilities${METNO_QT_SUFFIX}>=8.0.0
  metlibs-milogger>=6.0.0
)

SET(lib_name "metlibs-coserver${METNO_QT_SUFFIX}")

METNO_GEN_PKGCONFIG(metlibs-coserver.pc.in "${lib_name}.pc"
  "${METNO_PC_DEPS_QT}" "" "" "")

CONFIGURE_FILE(coserver.pc.in coserver.pc @ONLY)
INSTALL(FILES ${CMAKE_BINARY_DIR}/coserver.pc DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig")


ADD_SUBDIRECTORY(src)
