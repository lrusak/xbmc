#.rst:
# FindVulkan
# ------------
# Finds the VULKAN library
#
# This will define the following variables::
#
# VULKAN_FOUND - system has Vulkan
# VULKAN_INCLUDE_DIRS - the Vulkan include directory
# VULKAN_LIBRARIES - the Vulkan libraries
# VULKAN_DEFINITIONS - the Vulkan definitions

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_VULKAN vulkan>=1.1.126 QUIET)
endif()

find_path(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.hpp
                             PATHS "$ENV{VULKAN_SDK}/include"
                                   ${PC_VULKAN_INCLUDEDIR})

find_library(VULKAN_LIBRARY NAMES vulkan
                            PATHS "$ENV{VULKAN_SDK}/lib"
                                  ${PC_VULKAN_LIBDIR})

set(VULKAN_VERSION ${PC_VULKAN_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Vulkan
                                  REQUIRED_VARS VULKAN_LIBRARY VULKAN_INCLUDE_DIR
                                  VERSION_VAR VULKAN_VERSION)

if(VULKAN_FOUND)
  set(VULKAN_LIBRARIES ${VULKAN_LIBRARY})
  set(VULKAN_INCLUDE_DIRS ${VULKAN_INCLUDE_DIR})
  set(VULKAN_DEFINITIONS -DHAS_VULKAN=1)
endif()

mark_as_advanced(VULKAN_INCLUDE_DIR VULKAN_LIBRARY)

# shader stuff
find_program(GLSLANG_VALIDATOR glslangValidator)
if(NOT GLSLANG_VALIDATOR)
  message(ERROR "glslangValidator not found")
endif()

add_custom_target(generate-vulkan-shaders)

set(SHADER_SOURCE_DIR ${PROJECT_SOURCE_DIR}/system/shaders/vulkan)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders/vulkan)

set(SHADERS texture.vert
            texture_multi.vert
            texture.frag
            texture_fonts.frag
            texture_noblend.frag
            texture_multi.frag
            texture_multi_blendcolor.frag)

foreach(SHADER ${SHADERS})
  add_custom_command(COMMENT "Compiling shader: ${SHADER}"
                     OUTPUT ${SHADER}.inc
                     COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
                     COMMAND ${GLSLANG_VALIDATOR} -V -x -o ${SHADER_BINARY_DIR}/${SHADER}.inc ${SHADER_SOURCE_DIR}/${SHADER}
                     DEPENDS ${SHADER_SOURCE_DIR}/${SHADER} ${GLSLANG_VALIDATOR})
  add_custom_target(${SHADER} DEPENDS ${SHADER}.inc)
  add_dependencies(generate-vulkan-shaders ${SHADER})
endforeach()

include_directories(${SHADER_BINARY_DIR})
