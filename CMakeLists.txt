# ***** BEGIN LICENSE BLOCK *****
# This file is part of openfx-CMS,
#
# openfx-CMS is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# openfx-CMS is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with openfx-CMS.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
# ***** END LICENSE BLOCK *****

########################################################################
IF(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
MESSAGE(FATAL_ERROR "Prevented in-tree built. This is bad practice.")
ENDIF(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})

########################################################################
# Project setup
########################################################################
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)
PROJECT(OpenFXCMS CXX C)
ENABLE_TESTING()
set(CMAKE_CXX_STANDARD 17)

SET(CMAKE_VERBOSE_MAKEFILE 1)

# set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

# ========================== build platform ==========================

MESSAGE (STATUS "  Platform:")
MESSAGE (STATUS "    Host:"             ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_VERSION} ${CMAKE_HOST_SYSTEM_PROCESSOR})
if(CMAKE_CROSSCOMPILING)
  MESSAGE (STATUS "    Target:"         ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION} ${CMAKE_SYSTEM_PROCESSOR})
endif()
MESSAGE (STATUS "    CMake:"            ${CMAKE_VERSION})
MESSAGE (STATUS "    CMake generator:"  ${CMAKE_GENERATOR})
MESSAGE (STATUS "    CMake build tool:" ${CMAKE_BUILD_TOOL})
if(MSVC)
  MESSAGE (STATUS "    MSVC:"           ${MSVC_VERSION})
endif()
if(CMAKE_GENERATOR MATCHES Xcode)
  MESSAGE (STATUS "    Xcode:"          ${XCODE_VERSION})
endif()
if(NOT CMAKE_GENERATOR MATCHES "Xcode|Visual Studio")
  MESSAGE (STATUS "    Configuration:"  ${CMAKE_BUILD_TYPE})
endif()

SET(OPENFX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/openfx)

# C++ Include directories
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/SupportExt)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/RawLib)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/RawLib/mlv-lib)
INCLUDE_DIRECTORIES(${OPENFX_PATH}/include)
INCLUDE_DIRECTORIES(${OPENFX_PATH}/Support/include)
INCLUDE_DIRECTORIES(${OPENFX_PATH}/Support/Plugins/include)
INCLUDE_DIRECTORIES(${OpenCL_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/OpenCLBase)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/libs/mingw64/ceres/include)
INCLUDE_DIRECTORIES(utils)

include(FindOpenGL REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(OpenCL REQUIRED)

add_subdirectory(libs)

message("Oencl include dir: ${OpenCL_INCLUDE_DIRS}")
message("Oencl lib dir: ${OpenCL_LIBRARIES}")
message("pthread: ${PTHREAD}")

add_subdirectory(RawLib)

list(APPEND TARGET_DEFS OFX_EXTENSIONS_VEGAS OFX_EXTENSIONS_NUKE OFX_EXTENSIONS_NATRON
      OFX_EXTENSIONS_RESOLVE OFX_EXTENSIONS_TUTTLE OFX_SUPPORTS_OPENGLRENDER NOMINMAX OFX_SUPPORTS_MULTITHREAD)

add_subdirectory(openfx)
add_subdirectory(SupportExt)

# Use -Ofast when applicable (implies -ffast-math)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -Ofast -DNDEBUG")
    set (CMAKE_C_FLAGS_RELEASE   "-Wall -Wextra -Ofast -DNDEBUG")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
    set (CMAKE_CXX_FLAGS_RELEASE "-Wall -Ofast -DNDEBUG")
    set (CMAKE_C_FLAGS_RELEASE   "-Wall -Ofast -DNDEBUG")
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set (CMAKE_CXX_FLAGS_RELEASE "-Wall -Wextra -Ofast -DNDEBUG")
    set (CMAKE_C_FLAGS_RELEASE   "-Wall -Wextra -Ofast -DNDEBUG")
endif()

# Define "DEBUG" on debug builds
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# /MP on MSVC
ADD_COMPILE_OPTIONS($<$<CXX_COMPILER_ID:MSVC>:/MP>)

FILE(GLOB CMS_SOURCES
  "libs/GFX/gfx.cpp"
  "OpenCLBase/OpenCLBase.cpp"
  "CMSPattern/CMSPattern.cpp"
  "CMSBakeLut/CMSBakeLut.cpp"
  "CMSLogEncoding/CMSLogEncoding.cpp"
  "MLVReader/MLVReader.cpp"
  "CMSVectorScope/CMSVectorScope.cpp"
  "CMSColorConversion/CMSColorConversion.cpp"
)

FILE(GLOB CMS_RESOURCES
  "CMSPattern/net.sf.openfx.CMSPattern.png"
  "CMSPattern/net.sf.openfx.CMSPattern.svg"
  "MLVReader/net.sf.openfx.MLVReader.png"
)

FILE(GLOB CMS_OCLSHADERS
  "resources/opencl/debayer_ppg.cl"
  "resources/opencl/imgutils.cl"
)

IF(MINGW)
SET(LINKFLAGS "-static-libgcc -static-libstdc++")
ENDIF(MINGW)

ADD_LIBRARY(CMS SHARED ${CMS_SOURCES} $<TARGET_OBJECTS:rawlib_static> $<TARGET_OBJECTS:mlvlib_static> )
SET_TARGET_PROPERTIES(CMS PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(CMS PROPERTIES SUFFIX ".ofx")

# Add extra flags to the link step of the plugin
if(APPLE)
  SET_TARGET_PROPERTIES(CMS PROPERTIES LINK_FLAGS "-fvisibility=hidden -exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/openfx/Support/include/osxSymbols")
  SET_TARGET_PROPERTIES(CMS PROPERTIES INSTALL_RPATH "@loader_path/../Frameworks;@loader_path/../Libraries")
elseif(MINGW)
  SET_TARGET_PROPERTIES(CMS PROPERTIES LINK_FLAGS "${LINKFLAGS} -shared -fvisibility=hidden -Xlinker --version-script=${CMAKE_CURRENT_SOURCE_DIR}/openfx/Support/include/linuxSymbols")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
# Linux & FreeBSD
  SET_TARGET_PROPERTIES(CMS PROPERTIES LINK_FLAGS "-Wl,-fvisibility=hidden,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/openfx/Support/include/linuxSymbols")
  SET_TARGET_PROPERTIES(CMS PROPERTIES INSTALL_RPATH "$ORIGIN/../../Libraries")
endif()

INCLUDE_DIRECTORIES(CMS libs/GFX)
TARGET_COMPILE_DEFINITIONS(CMS PRIVATE ${TARGET_DEFS})
TARGET_LINK_LIBRARIES(CMS ${OPENGL_gl_LIBRARY} ${OpenCL_LIBRARIES} supportext-static openfx-static libraw_static m ws2_32 Ceres::ceres gomp) 

# Find and set the arch name.
# http://openeffects.org/documentation/reference/ch02s02.html
SET(OFX_ARCH UNKNOWN)
# Win
if(MINGW)
    set(OFX_ARCH "Win32")
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(OFX_ARCH "Win64")
    endif()
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64")
    set(OFX_ARCH "Win64")
    endif()
    message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR} ${OFX_ARCH}")
# Visual Studio    
elseif(MSVC)
    set(OFX_ARCH "Win32")
    if((${CMAKE_SYSTEM_PROCESSOR} STREQUAL "IA64") OR (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "AMD64"))
        set(OFX_ARCH "Win64")
    endif()
# FreeBSD
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set(OFX_ARCH "FreeBSD-x86")
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
        set(OFX_ARCH "FreeBSD-x86-64")
    endif()
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
        if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
            set(OFX_ARCH FreeBSD-arm-64)
        else()
            set(OFX_ARCH FreeBSD-arm)
        endif()
    endif()
# MacOS
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(OFX_ARCH "MacOS")
# Linux
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(OFX_ARCH "Linux-x86")
    if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
        set(OFX_ARCH "Linux-x86-64")
    endif()
    if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
        if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
            set(OFX_ARCH Linux-arm-64)
        else()
            set(OFX_ARCH Linux-arm)
        endif()
    endif()
endif()

IF("${OFX_ARCH}" EQUAL "UNKNOWN")
  MESSAGE(SEND_ERROR "OFX_ARCH_NAME was not detected. Install targets will be unavailable. See http://openeffects.org/documentation/reference/ch02s02.html to pick an arch name.")
ELSE()
  INSTALL(TARGETS CMS
    RUNTIME DESTINATION CMS.ofx.bundle/Contents/${OFX_ARCH}
    LIBRARY DESTINATION CMS.ofx.bundle/Contents/${OFX_ARCH}
  )

  INSTALL(FILES ${CMS_RESOURCES}
    DESTINATION CMS.ofx.bundle/Contents/Resources
  )

  INSTALL(FILES ${CMS_OCLSHADERS}
    DESTINATION CMS.ofx.bundle/Contents/Resources/Shaders
  )

  INSTALL(FILES Info.plist
    DESTINATION CMS.ofx.bundle/Contents
  )

  INSTALL(DIRECTORY resources/data
    DESTINATION CMS.ofx.bundle/Contents/Resources
  )
ENDIF()
