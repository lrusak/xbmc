#.rst:
# FindLibVncServer
# ----------
# Finds the LibVncServer library
#
# This will define the following variables::
#
# LIBVNCSERVER_FOUND - system has LibVncServer
# LIBVNCSERVER_INCLUDE_DIRS - the LibVncServer include directory
# LIBVNCSERVER_LIBRARIES - the LibVncServer libraries
#

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_LIBVNCSERVER libvncserver QUIET)
endif()

find_path(LIBVNCSERVER_INCLUDE_DIR NAMES rfb.h
                             PATH_SUFFIXES rfb
                             PATHS ${PC_LIBVNCSERVER_INCLUDEDIR})
find_library(LIBVNCSERVER_LIBRARY NAMES vncserver
                            PATHS ${PC_LIBVNCSERVER_LIBDIR})

set(LIBVNCSERVER_VERSION ${PC_LIBVNCSERVER_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibVncServer
                                  REQUIRED_VARS LIBVNCSERVER_LIBRARY LIBVNCSERVER_INCLUDE_DIR
                                  VERSION_VAR LIBVNCSERVER_VERSION)

if(LIBVNCSERVER_FOUND)
  set(LIBVNCSERVER_LIBRARIES ${LIBVNCSERVER_LIBRARY})
  set(LIBVNCSERVER_INCLUDE_DIRS ${LIBVNCSERVER_INCLUDE_DIR})
endif()

mark_as_advanced(LIBVNCSERVER_INCLUDE_DIR LIBVNCSERVER_LIBRARY)
