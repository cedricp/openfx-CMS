# ***** BEGIN LICENSE BLOCK *****
# This file is part of openfx-CMS <https://github.com/NatronGitHub/openfx-CMS>,
# Copyright (C) 2013-2018 INRIA
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

if(MINGW)
set(PC_LIBRAW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/mingw64")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

#set(CMAKE_INSTALL_PREFIX "E:/Applications/Natron/Plugins/OFX/Natron")
#set(CMAKE_INSTALL_PREFIX "C:/Users/cedric/Documents/ofx")

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

include(FindOpenGL REQUIRED)
find_package(LibRaw  REQUIRED)
find_package(Eigen3 REQUIRED)

message("Using libRAW ${LibRaw_LIBRARIES}")

add_subdirectory(RawLib)

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
SET(OPENFX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/openfx)

# Check that submodule have been initialized and updated
if(NOT EXISTS ${PROJECT_SOURCE_DIR}/openfx/include)
  message(FATAL_ERROR
    "\n submodule(s) are missing, please update your repository:\n"
    "  > git submodule update -i\n")
endif()

# C++ Include directories
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/SupportExt)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/RawLib)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/RawLib/mlv-lib)
INCLUDE_DIRECTORIES(${OPENFX_PATH}/include)
INCLUDE_DIRECTORIES(${OPENFX_PATH}/Support/include)
INCLUDE_DIRECTORIES(${OPENFX_PATH}/Support/Plugins/include)

# Define "DEBUG" on debug builds
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# /MP on MSVC
ADD_COMPILE_OPTIONS($<$<CXX_COMPILER_ID:MSVC>:/MP>)

SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

set(OFX_SUPPORT_HEADER_DIR "${OPENFX_PATH}/Support/include")
set(OFX_SUPPORT_LIBRARY_DIR "${OPENFX_PATH}/Support/Library")

FILE(GLOB SUPPORT_SOURCES
    "${OFX_SUPPORT_HEADER_DIR}/*.h"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsCore.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsImageEffect.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsInteract.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsLog.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsMultiThread.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsParams.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsProperty.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/ofxsPropertyValidation.cpp"
    "${OFX_SUPPORT_LIBRARY_DIR}/*.h"
    "SupportExt/tinythread.cpp"
    "SupportExt/ofxsThreadSuite.cpp"
    "SupportExt/ofxsLut.cpp"
    "SupportExt/*.h"
    "SupportExt/ofxsGenerator.cpp"
    "SupportExt/ofxsLut.cpp"
    "SupportExt/ofxsMultiPlane.cpp"
    "SupportExt/ofxsOGLTextRenderer.cpp"
    "SupportExt/ofxsOGLFontData.cpp"
    "SupportExt/ofxsRamp.cpp"
    "SupportExt/ofxsShutter.cpp"
    "SupportExt/ofxsTracking.cpp"
    "SupportExt/ofxsTransform3x3.cpp"
    "SupportExt/ofxsTransformInteract.cpp"
    "SupportExt/ofxsRectangleInteract.cpp"
    "SupportExt/*.h"
)

#ADD_LIBRARY(Support STATIC )
#TARGET_COMPILE_DEFINITIONS(Support PRIVATE OFX_SUPPORTS_OPENGLRENDER OFX_EXTENSIONS_TUTTLE OFX_EXTENSIONS_NUKE OFX_EXTENSIONS_NATRON OFX_EXTENSIONS_VEGAS NOMINMAX)

FILE(GLOB CMS_SOURCES
  "CMSPattern/CMSPattern.cpp"
  "CMSBakeLut/CMSBakeLut.cpp"
  "CMSLogEncoding/CMSLogEncoding.cpp"
  "CMSMLVReader/CMSMLVReader.cpp"
)

FILE(GLOB CMS_RESOURCES
  "CMSPattern/net.sf.openfx.CMSPattern.png"
  "CMSPattern/net.sf.openfx.CMSPattern.svg"
)

ADD_LIBRARY(CMS SHARED ${CMS_SOURCES} ${SUPPORT_SOURCES} $<TARGET_OBJECTS:rawlib_static> $<TARGET_OBJECTS:mlvlib_static>)
SET_TARGET_PROPERTIES(CMS PROPERTIES PREFIX "")
SET_TARGET_PROPERTIES(CMS PROPERTIES SUFFIX ".ofx")

# Add extra flags to the link step of the plugin
if(APPLE)
    set_target_properties(CMS PROPERTIES LINK_FLAGS "-fvisibility=hidden -exported_symbols_list,${OFX_SUPPORT_HEADER_DIR}/osxSymbols")
    set_target_properties(CMS PROPERTIES INSTALL_RPATH "@loader_path/../Frameworks;@loader_path/../Libraries")
elseif(MINGW)
    set_target_properties(CMS PROPERTIES LINK_FLAGS "-shared -fvisibility=hidden -Xlinker --version-script=${OFX_SUPPORT_HEADER_DIR}/linuxSymbols")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD" OR ${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
# Linux & FreeBSD
    set_target_properties(CMS PROPERTIES LINK_FLAGS "-Wl,-fvisibility=hidden,--version-script=${OFX_SUPPORT_HEADER_DIR}/linuxSymbols")
    set_target_properties(CMS PROPERTIES INSTALL_RPATH "$ORIGIN/../../Libraries")
endif()

TARGET_COMPILE_DEFINITIONS(CMS PRIVATE OFX_EXTENSIONS_VEGAS OFX_EXTENSIONS_NUKE OFX_EXTENSIONS_NATRON OFX_EXTENSIONS_TUTTLE OFX_SUPPORTS_OPENGLRENDER NOMINMAX OFX_SUPPORTS_MULTITHREAD)
TARGET_LINK_LIBRARIES(CMS ${OPENGL_gl_LIBRARY} $<LINK_LIBRARY:WHOLE_ARCHIVE,${LibRaw_LIBRARIES}> ws2_32 gomp) 

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

  INSTALL(FILES Info.plist
    DESTINATION CMS.ofx.bundle/Contents
  )
ENDIF()
