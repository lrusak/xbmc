include(${CORE_SOURCE_DIR}/cmake/scripts/common/Macros.cmake)

core_find_versions()

# configure_file without dependency tracking
# configure_file would register additional file dependencies that interfere
# with the ones from add_custom_command (and the generation would happen twice)
function(generate_versioned_file _SRC _DEST)
  file(READ ${CORE_SOURCE_DIR}/${_SRC} file_content)
  string(CONFIGURE "${file_content}" file_content @ONLY)
  file(WRITE ${CMAKE_BINARY_DIR}/${_DEST} "${file_content}")
endfunction()

# workaround for xbmc.json
file(STRINGS ${CORE_SOURCE_DIR}/xbmc/interfaces/json-rpc/schema/version.txt jsonrpc_version)

# add-on xml's
file(GLOB ADDON_XML_IN_FILE ${CORE_SOURCE_DIR}/addons/*/addon.xml.in)
foreach(loop_var ${ADDON_XML_IN_FILE})
  list(GET loop_var 0 xml_name)

  string(REPLACE "/addon.xml.in" "" source_dir ${xml_name})
  string(REPLACE ${CORE_SOURCE_DIR} ${CMAKE_BINARY_DIR} dest_dir ${source_dir})
  file(MAKE_DIRECTORY ${dest_dir})

  # copy everything except addon.xml.in to build folder
  file(COPY "${source_dir}" DESTINATION "${CMAKE_BINARY_DIR}/addons" REGEX ".xml.in" EXCLUDE)

  configure_file(${source_dir}/addon.xml.in ${dest_dir}/addon.xml @ONLY)

  unset(source_dir)
  unset(dest_dir)
  unset(xml_name)
endforeach()

generate_versioned_file(xbmc/CompileInfo.cpp.in ${CORE_BUILD_DIR}/xbmc/CompileInfo.cpp)
