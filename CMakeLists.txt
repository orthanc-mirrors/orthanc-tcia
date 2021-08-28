# TCIA plugin for Orthanc
# Copyright (C) 2021 Sebastien Jodogne, UCLouvain, Belgium
#
# This program is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.


cmake_minimum_required(VERSION 2.8)

project(OrthancTcia)

set(ORTHANC_PLUGIN_VERSION "mainline")

if (ORTHANC_PLUGIN_VERSION STREQUAL "mainline")
  set(ORTHANC_FRAMEWORK_DEFAULT_VERSION "mainline")
  set(ORTHANC_FRAMEWORK_DEFAULT_SOURCE "hg")
else()
  set(ORTHANC_FRAMEWORK_DEFAULT_VERSION "1.9.6")
  set(ORTHANC_FRAMEWORK_DEFAULT_SOURCE "web")
endif()


# Parameters of the build
set(STATIC_BUILD OFF CACHE BOOL "Static build of the third-party libraries (necessary for Windows)")
set(STANDALONE_BUILD ON CACHE BOOL "Standalone build (all the resources are embedded, necessary for releases)")
set(ALLOW_DOWNLOADS OFF CACHE BOOL "Allow CMake to download packages")
set(ORTHANC_FRAMEWORK_SOURCE "${ORTHANC_FRAMEWORK_DEFAULT_SOURCE}" CACHE STRING "Source of the Orthanc framework (can be \"system\", \"hg\", \"archive\", \"web\" or \"path\")")
set(ORTHANC_FRAMEWORK_VERSION "${ORTHANC_FRAMEWORK_DEFAULT_VERSION}" CACHE STRING "Version of the Orthanc framework")
set(ORTHANC_FRAMEWORK_ARCHIVE "" CACHE STRING "Path to the Orthanc archive, if ORTHANC_FRAMEWORK_SOURCE is \"archive\"")
set(ORTHANC_FRAMEWORK_ROOT "" CACHE STRING "Path to the Orthanc source directory, if ORTHANC_FRAMEWORK_SOURCE is \"path\"")


# Advanced parameters to fine-tune linking against system libraries
set(USE_SYSTEM_ORTHANC_SDK ON CACHE BOOL "Use the system version of the Orthanc plugin SDK")
set(USE_SYSTEM_LIBCSV ON CACHE BOOL "Use the system version of libcsv")
set(ORTHANC_FRAMEWORK_STATIC OFF CACHE BOOL "If linking against the Orthanc framework system library, indicates whether this library was statically linked")
mark_as_advanced(ORTHANC_FRAMEWORK_STATIC)


# Download and setup the Orthanc framework
include(${CMAKE_SOURCE_DIR}/Resources/Orthanc/CMake/DownloadOrthancFramework.cmake)

if (ORTHANC_FRAMEWORK_SOURCE STREQUAL "system")
  if (ORTHANC_FRAMEWORK_USE_SHARED)
    include(FindBoost)
    find_package(Boost COMPONENTS thread)
    
    if (NOT Boost_FOUND)
      message(FATAL_ERROR "Unable to locate Boost on this system")
    endif()
    
    link_libraries(${Boost_LIBRARIES} jsoncpp)
  endif()

  link_libraries(${ORTHANC_FRAMEWORK_LIBRARIES})
else()
  include(${ORTHANC_FRAMEWORK_ROOT}/../Resources/CMake/OrthancFrameworkParameters.cmake)
  
  set(ENABLE_LOCALE OFF)         # Disable support for locales (notably in Boost)
  set(ENABLE_SQLITE ON)
  set(ENABLE_MODULE_JOBS OFF CACHE INTERNAL "")
  set(ENABLE_MODULE_DICOM OFF CACHE INTERNAL "")
  
  include(${ORTHANC_FRAMEWORK_ROOT}/../Resources/CMake/OrthancFrameworkConfiguration.cmake)
  include_directories(${ORTHANC_FRAMEWORK_ROOT})
endif()


include(${CMAKE_SOURCE_DIR}/Resources/CMake/LibCsvConfiguration.cmake)
include(${CMAKE_SOURCE_DIR}/Resources/CMake/WebApplicationResources.cmake)
include(${CMAKE_SOURCE_DIR}/Resources/Orthanc/Plugins/OrthancPluginsExports.cmake)


# Check that the Orthanc SDK headers are available
if (STATIC_BUILD OR NOT USE_SYSTEM_ORTHANC_SDK)
  include_directories(${CMAKE_SOURCE_DIR}/Resources/Orthanc/Sdk-1.4.2)
else ()
  CHECK_INCLUDE_FILE_CXX(orthanc/OrthancCPlugin.h HAVE_ORTHANC_H)
  if (NOT HAVE_ORTHANC_H)
    message(FATAL_ERROR "Please install the headers of the Orthanc plugins SDK")
  endif()
endif()


if (STANDALONE_BUILD)
  add_definitions(
    -DORTHANC_STANDALONE=1
    )
  set(TCIA_EMBEDDED_RESOURCES
    TCIA_HTML  ${CMAKE_SOURCE_DIR}/WebApplication/index.html
    TCIA_JS    ${CMAKE_SOURCE_DIR}/WebApplication/app.js
    )
else()
  add_definitions(
    -DORTHANC_STANDALONE=0
    -DTCIA_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
    )
endif()

add_definitions(
  -DHAS_ORTHANC_EXCEPTION=1
  -DORTHANC_PLUGIN_VERSION="${ORTHANC_PLUGIN_VERSION}"
  -DTCIA_BASE_URL="https://services.cancerimagingarchive.net/services/v4/TCIA/query"
  )

EmbedResources(
  ${TCIA_EMBEDDED_RESOURCES}
  ${WEB_APPLICATION_RESOURCES}
  NBIA_EXPORT          ${CMAKE_SOURCE_DIR}/WebApplication/images/nbia-export.png
  ORTHANC_EXPLORER_JS  ${CMAKE_SOURCE_DIR}/Resources/OrthancExplorer.js
  ORTHANC_LOGO         ${CMAKE_SOURCE_DIR}/WebApplication/images/orthanc-logo.png
  TCIA_LOGO            ${CMAKE_SOURCE_DIR}/WebApplication/images/tcia-logo.png
  )

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "kFreeBSD" OR
    ${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  link_libraries(rt)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
  execute_process(
    COMMAND 
    ${PYTHON_EXECUTABLE} ${ORTHANC_FRAMEWORK_ROOT}/../Resources/WindowsResources.py
    ${ORTHANC_PLUGIN_VERSION} "OrthancTcia" OrthancTcia.dll "Interface between The Cancer Imaging Archive and Orthanc"
    ERROR_VARIABLE Failure
    OUTPUT_FILE ${AUTOGENERATED_DIR}/Version.rc
    )

  if (Failure)
    message(FATAL_ERROR "Error while computing the version information: ${Failure}")
  endif()

  list(APPEND AUTOGENERATED_SOURCES  ${AUTOGENERATED_DIR}/Version.rc)
endif()


add_custom_target(
  AutogeneratedTarget
  DEPENDS 
  ${AUTOGENERATED_SOURCES}
  )
          
add_library(OrthancTcia SHARED
  ${AUTOGENERATED_SOURCES}
  ${CMAKE_SOURCE_DIR}/Plugin/CsvParser.cpp
  ${CMAKE_SOURCE_DIR}/Plugin/HttpCache.cpp
  ${CMAKE_SOURCE_DIR}/Plugin/Plugin.cpp
  ${CMAKE_SOURCE_DIR}/Plugin/TciaImportJob.cpp
  ${CMAKE_SOURCE_DIR}/Resources/Orthanc/Plugins/OrthancPluginCppWrapper.cpp
  ${LIBCSV_SOURCES}
  ${ORTHANC_CORE_SOURCES}
  )

add_dependencies(OrthancTcia AutogeneratedTarget)


message("Setting the version of the library to ${ORTHANC_PLUGIN_VERSION}")

set_target_properties(OrthancTcia PROPERTIES 
  VERSION ${ORTHANC_PLUGIN_VERSION} 
  SOVERSION ${ORTHANC_PLUGIN_VERSION})

install(
  TARGETS OrthancTcia
  RUNTIME DESTINATION lib    # Destination for Windows
  LIBRARY DESTINATION share/orthanc/plugins    # Destination for Linux
  )
