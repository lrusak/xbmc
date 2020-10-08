if(NOT CORE_SYSTEM_NAME)
  string(TOLOWER ${CMAKE_SYSTEM_NAME} CORE_SYSTEM_NAME)
endif()

if(CORE_SYSTEM_NAME STREQUAL linux OR CORE_SYSTEM_NAME STREQUAL freebsd)
  # Set default CORE_PLATFORM_NAME to X11
  # This is overridden by user setting -DCORE_PLATFORM_NAME=<platform>
  set(_DEFAULT_PLATFORM X11)
  option(ENABLE_APP_AUTONAME    "Enable renaming the binary according to windowing?" ON)
else()
  string(TOLOWER ${CORE_SYSTEM_NAME} _DEFAULT_PLATFORM)
endif()

set(APP_BINARY_SUFFIX ".bin")

#
# Note: please do not use CORE_PLATFORM_NAME in any checks,
# use the normalized to lower case CORE_PLATFORM_NAME_LC (see below) instead
#
if(NOT CORE_PLATFORM_NAME)
  set(CORE_PLATFORM_NAME ${_DEFAULT_PLATFORM})
endif()
set(CORE_PLATFORM_NAME ${CORE_PLATFORM_NAME} CACHE STRING "Platform port to build" FORCE)
unset(_DEFAULT_PLATFORM)

list(APPEND final_message "Platforms: ${CORE_PLATFORM_NAME}")
string(REPLACE " " ";" CORE_PLATFORM_NAME "${CORE_PLATFORM_NAME}")
foreach(platform IN LISTS CORE_PLATFORM_NAME)
  string(TOLOWER ${platform} platform)
  list(APPEND CORE_PLATFORM_NAME_LC ${platform})
endforeach()

list(LENGTH CORE_PLATFORM_NAME_LC PLATFORM_COUNT)
if(PLATFORM_COUNT EQUAL 1)
  if(ENABLE_APP_AUTONAME)
    set(APP_BINARY_SUFFIX "-${CORE_PLATFORM_NAME_LC}")
  endif()
endif()

foreach(CORE_PLATFORM_LC IN LISTS CORE_PLATFORM_NAME_LC)
  if(EXISTS ${CMAKE_SOURCE_DIR}/cmake/platform/${CORE_SYSTEM_NAME}/${CORE_PLATFORM_LC}.cmake)
    include(${CMAKE_SOURCE_DIR}/cmake/platform/${CORE_SYSTEM_NAME}/${CORE_PLATFORM_LC}.cmake)
  else()
    file(GLOB _platformnames RELATIVE ${CMAKE_SOURCE_DIR}/cmake/platform/${CORE_SYSTEM_NAME}/
                                      ${CMAKE_SOURCE_DIR}/cmake/platform/${CORE_SYSTEM_NAME}/*.cmake)
    string(REPLACE ".cmake" " " _platformnames ${_platformnames})
    message(FATAL_ERROR "invalid CORE_PLATFORM_NAME: ${CORE_PLATFORM_NAME_LC}\nValid platforms: ${_platformnames}")
  endif()
endforeach()
