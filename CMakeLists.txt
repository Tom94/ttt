# This file was developed by Thomas Müller <contact@tom94.net>.
# It is published under the GPLv3 License. See the LICENSE file.

cmake_minimum_required(VERSION 3.10)
project(
	ttt
	VERSION 0.1
	DESCRIPTION "Tom's typing test tool"
	LANGUAGES C CXX
)
set(TTT_VERSION "${CMAKE_PROJECT_VERSION}")
if (NOT TTT_DEPLOY)
    set(TTT_VERSION "${TTT_VERSION}dev")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TTT_VERSION_ARCH "${TTT_VERSION} (64 bit)")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(TTT_VERSION_ARCH "${TTT_VERSION} (32 bit)")
endif()

set(TTT_DEFINITIONS -DTTT_VERSION="${TTT_VERSION_ARCH}")

if (CMAKE_EXPORT_COMPILE_COMMANDS)
	set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

# Set a default configuration if none was specified
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "No release type specified. Setting to 'Release'.")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

if (APPLE)
	set(CMAKE_MACOSX_RPATH ON)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include word and quote lists
include("${CMAKE_CURRENT_SOURCE_DIR}/dependencies/cmrc/CMakeRC.cmake")
file(GLOB TTT_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/resources/**/*")
cmrc_add_resource_library(ttt-resources NAMESPACE ttt ${TTT_RESOURCES})
list(APPEND TTT_LIBRARIES ttt-resources)

add_executable(ttt src/main.cpp
	dependencies/unilib/unilib/unicode.cpp
	dependencies/unilib/unilib/uninorms.cpp
	dependencies/unilib/unilib/unistrip.cpp
)

target_compile_definitions(ttt PRIVATE ${TTT_DEFINITIONS})
target_include_directories(ttt PRIVATE dependencies/unilib)
target_link_libraries(ttt PRIVATE ${TTT_LIBRARIES})

install(TARGETS ttt)
