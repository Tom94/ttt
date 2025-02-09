# This file was developed by Thomas Müller <contact@tom94.net>.
# It is published under the GPLv3 License. See the LICENSE file.

cmake_minimum_required(VERSION 3.10)
project(
	ttt
	VERSION 0.1
	DESCRIPTION "Tom's typing test tool"
	LANGUAGES C CXX
)

# Set ourselves as the startup project in visual studio. Not available until cmake 3.6, but doesn't break older versions.
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ttt)

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

if (MSVC)
# Disable annoying secure CRT warnings
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS")

# Parallel build
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP24")

# Static build
	set(CompilerFlags
		CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
		CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
		CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
		CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
	)

	foreach (CompilerFlag ${CompilerFlags})
		string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
	endforeach()

	if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()

	# ttt uses UTF-8 chars. Specify UTF-8 charset explicitly.
	# "/utf-8" is equivalent to "/source-charset:utf-8 /execution-charset:utf-8".
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

	# Disable warnings that are present in dependencies and irrelevant to us
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4100") # unused arguments
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd5054") # deprecated enum & operator

	# To allow for wildcards in command-line path arguments on windows,
	# we need to link to wsetargv.obj
	# http://msdn.microsoft.com/en-us/library/8bch7bkk.aspx
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} wsetargv.obj")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_ZLIB=1 -s DISABLE_EXCEPTION_CATCHING=0")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
	if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-anonymous-struct -Wno-c99-extensions -Wno-nested-anon-types -Wno-deprecated-register -Wno-deprecated-anon-enum-enum-conversion")
	endif()
	if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-misleading-indentation -Wno-deprecated-declarations")
	endif()
endif()

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
target_include_directories(ttt PRIVATE dependencies dependencies/unilib)
target_link_libraries(ttt PRIVATE ${TTT_LIBRARIES})

install(TARGETS ttt)
