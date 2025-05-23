#
# This file is part of the WarheadCore Project. See AUTHORS file for Copyright information
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Affero General Public License as published by the
# Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <http://www.gnu.org/licenses/>.
#

# Make the script module list available in the current scope
GetModuleSourceList(MODULES_MODULE_LIST)

# Make the native install offset available in this scope
GetInstallOffset(INSTALL_OFFSET)

# Sets the MODULES_${SOURCE_MODULE} variables
# when using predefined templates for script building
# like dynamic, static
# Sets MODULES_DEFAULT_LINKAGE
if(MODULES MATCHES "dynamic")
  set(MODULES_DEFAULT_LINKAGE "dynamic")
elseif(MODULES MATCHES "static")
  set(MODULES_DEFAULT_LINKAGE "static")
else()
  set(MODULES_DEFAULT_LINKAGE "disabled")
endif()

set(MOD_ELUNA_FOUND 0)
set(MOD_ELUNA_PATH "")

function(ConfigureModEluna projectName projectPath)
  if (APPLE)
    target_compile_definitions(${projectName}
      PUBLIC
        LUA_USE_MACOSX)
  elseif (UNIX)
    target_compile_definitions(${projectName}
      PUBLIC
        LUA_USE_LINUX)
  endif()

  # Define eluna compile options
  target_compile_options(${projectName}
    PUBLIC
      -DWARHEADCORE
      -DWOTLK)

  if (WIN32)
    if (MSVC)
      set(MSVC_CONFIGURATION_NAME $(ConfigurationName)/)
    endif()

    add_custom_command(TARGET ${projectName} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/${MSVC_CONFIGURATION_NAME}lua_scripts/extensions/"
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${projectPath}/LuaEngine/extensions" "${CMAKE_BINARY_DIR}/bin/${MSVC_CONFIGURATION_NAME}lua_scripts/extensions/")
  endif()

  install(DIRECTORY "${projectPath}/LuaEngine/extensions" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/lua_scripts/")
endfunction()

# Set the MODULES_${SOURCE_MODULE} variables from the
# variables set above
foreach(SOURCE_MODULE ${MODULES_MODULE_LIST})
  ModuleNameToVariable(${SOURCE_MODULE} MODULE_MODULE_VARIABLE)

  if(${MODULE_MODULE_VARIABLE} STREQUAL "default")
    set(${MODULE_MODULE_VARIABLE} ${MODULES_DEFAULT_LINKAGE})
  endif()

  # Build the Graph values
  if(${MODULE_MODULE_VARIABLE} MATCHES "dynamic")
    GetProjectNameOfModuleName(${SOURCE_MODULE} MODULE_SOURCE_PROJECT_NAME)
    GetNativeSharedLibraryName(${MODULE_SOURCE_PROJECT_NAME} MODULE_PROJECT_LIBRARY)
    list(APPEND MODULE_GRAPH_KEYS ${MODULE_SOURCE_PROJECT_NAME})
    set(MODULE_GRAPH_VALUE_DISPLAY_${MODULE_SOURCE_PROJECT_NAME} ${MODULE_PROJECT_LIBRARY})
    list(APPEND MODULE_GRAPH_VALUE_CONTAINS_MODULES_${MODULE_SOURCE_PROJECT_NAME} ${SOURCE_MODULE})
  elseif(${MODULE_MODULE_VARIABLE} MATCHES "static")
    list(APPEND MODULE_GRAPH_KEYS worldserver)
    set(MODULE_GRAPH_VALUE_DISPLAY_worldserver worldserver)
    list(APPEND MODULE_GRAPH_VALUE_CONTAINS_MODULES_worldserver ${SOURCE_MODULE})
  else()
    list(APPEND MODULE_GRAPH_KEYS disabled)
    set(MODULE_GRAPH_VALUE_DISPLAY_disabled disabled)
    list(APPEND MODULE_GRAPH_VALUE_CONTAINS_MODULES_disabled ${SOURCE_MODULE})
  endif()
endforeach()

list(SORT MODULE_GRAPH_KEYS)
list(REMOVE_DUPLICATES MODULE_GRAPH_KEYS)

# Display the module graph
message("* Modules configuration (${MODULES}):")
message("  |")

foreach(MODULE_GRAPH_KEY ${MODULE_GRAPH_KEYS})
if(NOT MODULE_GRAPH_KEY STREQUAL "disabled")
  message("  +- ${MODULE_GRAPH_VALUE_DISPLAY_${MODULE_GRAPH_KEY}}")
else()
  message("  |  ${MODULE_GRAPH_VALUE_DISPLAY_${MODULE_GRAPH_KEY}}")
endif()
foreach(MODULE_GRAPH_PROJECT_ENTRY ${MODULE_GRAPH_VALUE_CONTAINS_MODULES_${MODULE_GRAPH_KEY}})
  message("  |   +- ${MODULE_GRAPH_PROJECT_ENTRY}")
endforeach()
message("  |")
endforeach()

message("")

# Base sources which are used by every script project
if (USE_SCRIPTPCH)
  set(PRIVATE_PCH_HEADER ModulesPCH.h)
endif()

GroupSources(${CMAKE_CURRENT_SOURCE_DIR})

# Configures the scriptloader with the given name and stores the output in the LOADER_OUT variable.
# It is possible to expose multiple subdirectories from the same scriptloader through passing
# it to the variable arguments
function(ConfigureScriptLoader SCRIPTLOADER_NAME LOADER_OUT IS_DYNAMIC_SCRIPTLOADER)
  # Deduces following variables which are referenced by thge template:
  # WARHEAD_IS_DYNAMIC_SCRIPTLOADER
  # WARHEAD_SCRIPTS_FORWARD_DECL
  # WARHEAD_SCRIPTS_INVOKE
  # WARHEAD_CURRENT_SCRIPT_PROJECT

  # To generate export macros
  set(WARHEAD_IS_DYNAMIC_SCRIPTLOADER ${IS_DYNAMIC_SCRIPTLOADER})

  # To generate forward declarations of the loading functions
  unset(WARHEAD_SCRIPTS_FORWARD_DECL)
  unset(WARHEAD_SCRIPTS_INVOKE)

  # The current script project which is built in
  set(WARHEAD_CURRENT_SCRIPT_PROJECT ${SCRIPTLOADER_NAME})

  foreach(LOCALE_SCRIPT_MODULE ${ARGN})

    # Replace bad words
    string(REGEX REPLACE - "_" LOCALE_SCRIPT_MODULE ${LOCALE_SCRIPT_MODULE})

    # Determine the loader function ("Add##${NameOfDirectory}##Scripts()")
    set(LOADER_FUNCTION
      "Add${LOCALE_SCRIPT_MODULE}Scripts()")

    # Generate the funciton call and the forward declarations
    set(WARHEAD_SCRIPTS_FORWARD_DECL
      "${WARHEAD_SCRIPTS_FORWARD_DECL}void ${LOADER_FUNCTION};\n")

    set(WARHEAD_SCRIPTS_INVOKE
      "${WARHEAD_SCRIPTS_INVOKE}    ${LOADER_FUNCTION};\n")
  endforeach()

  set(GENERATED_LOADER ${CMAKE_CURRENT_BINARY_DIR}/gen_scriptloader/${SCRIPTLOADER_NAME}/ModulesLoader.cpp)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ModulesLoader.cpp.in.cmake ${GENERATED_LOADER})
  set(${LOADER_OUT} ${GENERATED_LOADER} PARENT_SCOPE)
endfunction()

# Generates the actual module projects
# Fills the STATIC_SCRIPT_MODULES and DYNAMIC_SCRIPT_MODULE_PROJECTS variables
# which contain the names which scripts are linked statically/dynamically and
# adds the sources of the static modules to the PRIVATE_SOURCES_MODULES variable.
foreach(SOURCE_MODULE ${MODULES_MODULE_LIST})
  GetPathToModuleSource(${SOURCE_MODULE} MODULE_SOURCE_PATH)
  ModuleNameToVariable(${SOURCE_MODULE} MODULE_MODULE_VARIABLE)

  if(NOT (${MODULE_MODULE_VARIABLE} STREQUAL "disabled"))
    list(APPEND MODULE_LIST__ ${SOURCE_MODULE})
  endif()

  if((${MODULE_MODULE_VARIABLE} STREQUAL "disabled") OR
      (${MODULE_MODULE_VARIABLE} STREQUAL "static"))

    # Uninstall disabled modules
    GetProjectNameOfModuleName(${SOURCE_MODULE} MODULE_SOURCE_PROJECT_NAME)
    GetNativeSharedLibraryName(${MODULE_SOURCE_PROJECT_NAME} SCRIPT_MODULE_OUTPUT_NAME)
    list(APPEND DISABLED_SCRIPT_MODULE_PROJECTS ${INSTALL_OFFSET}/${SCRIPT_MODULE_OUTPUT_NAME})
    if(${MODULE_MODULE_VARIABLE} STREQUAL "static")

      # Add the module content to the whole static module
      CollectSourceFiles(${MODULE_SOURCE_PATH} PRIVATE_SOURCES_MODULES)
      CollectIncludeDirectories(${MODULE_SOURCE_PATH} PUBLIC_INCLUDES)

      # Disable build specific files for mod eluna
      if (${SOURCE_MODULE} STREQUAL "mod-eluna")
        list(REMOVE_ITEM PRIVATE_SOURCES_MODULES ${MODULE_SOURCE_PATH}/lualib/lua.c)
        list(REMOVE_ITEM PRIVATE_SOURCES_MODULES ${MODULE_SOURCE_PATH}/lualib/luac.c)
        set(MOD_ELUNA_FOUND 1)
        set(MOD_ELUNA_PATH ${MODULE_SOURCE_PATH})
      endif()

      # Add the module name to STATIC_SCRIPT_MODULES
      list(APPEND STATIC_SCRIPT_MODULES ${SOURCE_MODULE})

    endif()
  elseif(${MODULE_MODULE_VARIABLE} STREQUAL "dynamic")

    # Generate an own dynamic module which is loadable on runtime
    # Add the module content to the whole static module
    unset(MODULE_SOURCE_PRIVATE_SOURCES)
    CollectSourceFiles(${MODULE_SOURCE_PATH} MODULE_SOURCE_PRIVATE_SOURCES)
    CollectIncludeDirectories(${MODULE_SOURCE_PATH} PUBLIC_INCLUDES)

    # Disable build specific files for mod eluna
    if (${SOURCE_MODULE} STREQUAL "mod-eluna")
      list(REMOVE_ITEM MODULE_SOURCE_PRIVATE_SOURCES ${MODULE_SOURCE_PATH}/lualib/lua.c)
      list(REMOVE_ITEM MODULE_SOURCE_PRIVATE_SOURCES ${MODULE_SOURCE_PATH}/lualib/luac.c)
    endif()

    # Configure the scriptloader
    ConfigureScriptLoader(${SOURCE_MODULE} SCRIPT_MODULE_PRIVATE_SCRIPTLOADER ON ${SOURCE_MODULE})
    GetProjectNameOfModuleName(${SOURCE_MODULE} MODULE_SOURCE_PROJECT_NAME)

    # Add the module name to DYNAMIC_SCRIPT_MODULES
    list(APPEND DYNAMIC_SCRIPT_MODULE_PROJECTS ${MODULE_SOURCE_PROJECT_NAME})

    # Create the script module project
    add_library(${MODULE_SOURCE_PROJECT_NAME} SHARED
      ${MODULE_SOURCE_PRIVATE_SOURCES}
      ${SCRIPT_MODULE_PRIVATE_SCRIPTLOADER})

    target_link_libraries(${MODULE_SOURCE_PROJECT_NAME}
      PRIVATE
        warhead-core-interface
      PUBLIC
        game)

    target_include_directories(${MODULE_SOURCE_PROJECT_NAME}
      PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PUBLIC_INCLUDES})

    set_target_properties(${MODULE_SOURCE_PROJECT_NAME}
      PROPERTIES
        FOLDER
          "modules")

    if(UNIX)
      install(TARGETS ${MODULE_SOURCE_PROJECT_NAME}
        DESTINATION ${INSTALL_OFFSET} COMPONENT ${MODULE_SOURCE_PROJECT_NAME})
    elseif(WIN32)
      install(TARGETS ${MODULE_SOURCE_PROJECT_NAME}
        RUNTIME DESTINATION ${INSTALL_OFFSET} COMPONENT ${MODULE_SOURCE_PROJECT_NAME})
      if(MSVC)
        # Place the script modules in the script subdirectory
        set_target_properties(${MODULE_SOURCE_PROJECT_NAME} PROPERTIES
          RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug/scripts
          RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release/scripts
          RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/RelWithDebInfo/scripts
          RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin/MinSizeRel/scripts)
      endif()
    endif()

    # Disable build specific files for mod eluna
    if (${SOURCE_MODULE} STREQUAL "mod-eluna")
      ConfigureModEluna(${MODULE_SOURCE_PROJECT_NAME} ${MODULE_SOURCE_PATH})
    endif()
  else()
    message(FATAL_ERROR "Unknown value \"${${MODULE_MODULE_VARIABLE}}\" for module (${SOURCE_MODULE})!")
  endif()
endforeach()

# Add the dynamic script modules to the worldserver as dependency
set(WORLDSERVER_DYNAMIC_SCRIPT_MODULES_DEPENDENCIES ${DYNAMIC_SCRIPT_MODULE_PROJECTS} PARENT_SCOPE)

ConfigureScriptLoader("static" SCRIPT_MODULE_PRIVATE_SCRIPTLOADER OFF ${STATIC_SCRIPT_MODULES})

list(REMOVE_DUPLICATES SCRIPT_MODULE_PRIVATE_SCRIPTLOADER)

add_library(modules STATIC
  ModulesScriptLoader.h
  ${SCRIPT_MODULE_PRIVATE_SCRIPTLOADER}
  ${PRIVATE_SOURCES_MODULES})

target_link_libraries(modules
  PRIVATE
    warhead-core-interface
  PUBLIC
    game-interface)

target_include_directories(modules
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PUBLIC_INCLUDES})

# Enables Devs to Include a cmake file in their module that will get run inline with the config.
foreach(SOURCE_MODULE ${MODULES_MODULE_LIST})
    include("${CMAKE_SOURCE_DIR}/modules/${SOURCE_MODULE}/${SOURCE_MODULE}.cmake" OPTIONAL)
endforeach()

set_target_properties(modules
  PROPERTIES
    FOLDER
      "modules")

# Generate precompiled header
# if (USE_SCRIPTPCH)
#   list(APPEND ALL_SCRIPT_PROJECTS modules ${DYNAMIC_SCRIPT_MODULE_PROJECTS})
#   add_cxx_pch("${ALL_SCRIPT_PROJECTS}" ${PRIVATE_PCH_HEADER})
# endif()

# Remove all shared libraries in the installl directory which
# are contained in the static library already.
if (DISABLED_SCRIPT_MODULE_PROJECTS)
  install(CODE "
    foreach(SCRIPT_TO_UNINSTALL ${DISABLED_SCRIPT_MODULE_PROJECTS})
      if(EXISTS \"\${SCRIPT_TO_UNINSTALL}\")
        message(STATUS \"Uninstalling: \${SCRIPT_TO_UNINSTALL}\")
        file(REMOVE \"\${SCRIPT_TO_UNINSTALL}\")
      endif()
    endforeach()
  ")
endif()

# Stores the absolut path of the given config module in the variable
function(GetPathToModuleConfig module variable)
  set(${variable} "${CMAKE_SOURCE_DIR}/modules/${module}/conf" PARENT_SCOPE)
endfunction()

message(STATUS "* Modules config list:")
message(STATUS "  |")

foreach(ModuleName ${MODULE_LIST__})
  GetPathToModuleConfig(${ModuleName} MODULE_CONFIG_PATH)

  set(MODULE_LIST ${MODULE_LIST}${ModuleName},)

  file(GLOB MODULE_CONFIG_LIST RELATIVE
    ${MODULE_CONFIG_PATH}
    ${MODULE_CONFIG_PATH}/*.conf.dist)

  message(STATUS "  +- ${ModuleName}")

  foreach(configFileName ${MODULE_CONFIG_LIST})
    CopyModuleConfig("${MODULE_CONFIG_PATH}/${configFileName}")
    set(CONFIG_LIST ${CONFIG_LIST}${configFileName},)
    message(STATUS "  |  * ${configFileName}")
  endforeach()
endforeach()

# Define modules list
target_compile_options(modules
  INTERFACE
    -DWH_MODULES_LIST=$<1:"${MODULE_LIST}">)

# Define modules config list
target_compile_options(modules
  INTERFACE
    -DCONFIG_FILE_LIST=$<1:"${CONFIG_LIST}">)

if (MOD_ELUNA_FOUND)
  ConfigureModEluna(modules ${MOD_ELUNA_PATH})
endif()

message("")
