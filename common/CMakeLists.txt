cmake_minimum_required(VERSION 3.5)
project(liblgproxy_common LANGUAGES C CXX)

include(CommonDirectories.cmake)

add_definitions(-DLP_LOG_USE_COLOR)

# Check if submodules have been initialized
find_package(Git QUIET)
message(STATUS "Checking submodules")
if(GIT_FOUND AND EXISTS "${PROJECT_TOP}/.git")
    option(GIT_SUBMODULE "Check submodules before build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Fetching submodules")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init repos/LookingGlass repos/netfr
                        WORKING_DIRECTORY ${PROJECT_TOP}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT EXISTS "${PROJECT_TOP}/repos")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

add_compile_options(
    "-Wall"
    "-Wextra"
    "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
)

set(SOURCES
    src/lp_config.cpp
    src/lp_mem.cpp
)

add_library(lgproxy_common STATIC ${SOURCES})
set_property(TARGET lgproxy_common PROPERTY C_STANDARD 11)
set_property(TARGET lgproxy_common PROPERTY CXX_STANDARD 11)

target_link_libraries(lgproxy_common 
    lgproxy_log 
    tcm 
    netfr
    lgmp 
    lg_common
    m
)

target_include_directories(lgproxy_common
    PUBLIC
        include
    PRIVATE
        src
)

# Get Looking Glass version

execute_process(
    COMMAND sh -c "git submodule | grep -i LookingGlass | awk -F'[()]' '{print $2}'"
    WORKING_DIRECTORY "${PROJECT_TOP}"
    OUTPUT_VARIABLE LG_GIT_REV
    ERROR_QUIET
)

if ("${LG_GIT_REV}" STREQUAL "")
	set(LG_GIT_REV "UNKNOWN")
endif()

set(BUILD_VERSION_COMMENT
"\
/* This value is generated at compile time: do not modify it. To use a \n\
 * different version of Looking Glass or TCM, update the submodule version \n\
 * manually, and this value will be changed when recompiling. */")

string(STRIP "${LG_GIT_REV}" LGPROXY_LG_GIT_VER)

set(BUILD_VERSION 
"\
${BUILD_VERSION_COMMENT}\n\
static const char * LG_BUILD_VERSION = \"${LGPROXY_LG_GIT_VER}\";"
)

if(EXISTS "include/lg_build_version.h")
	file(READ include/lg_build_version.h LG_BUILD_VERSION_)
else()
	set(LG_BUILD_VERSION_ "")
endif()

if (NOT "${BUILD_VERSION}" STREQUAL "${LG_BUILD_VERSION_}")
	file(WRITE include/lg_build_version.h "${BUILD_VERSION}\n")
endif()

# Get LGProxy version

execute_process(
    COMMAND sh -c "git rev-parse --short HEAD"
    WORKING_DIRECTORY "${PROJECT_TOP}"
    OUTPUT_VARIABLE LP_GIT_REV
    ERROR_QUIET
)

if ("${LP_GIT_REV}" STREQUAL "")
	set(LP_GIT_REV "UNKNOWN")
endif()

string(STRIP "${LP_GIT_REV}" PROXY_GIT_VER)

set(BUILD_VERSION 
"\
${BUILD_VERSION_COMMENT}\n\
static const char * LP_BUILD_VERSION = \"${PROXY_GIT_VER}\";"
)

if(EXISTS "include/lp_build_version.h")
	file(READ include/lp_build_version.h LP_BUILD_VERSION_)
else()
	set(LP_BUILD_VERSION_ "")
endif()

if (NOT "${BUILD_VERSION}" STREQUAL "${LP_BUILD_VERSION_}")
	file(WRITE include/lp_build_version.h "${BUILD_VERSION}")
endif()