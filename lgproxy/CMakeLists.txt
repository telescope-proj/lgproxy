cmake_minimum_required(VERSION 3.5.0)
project(lgproxy LANGUAGES C)

include(CheckCCompilerFlag)
include(FeatureSummary)

get_filename_component(LGPROXY_TOP "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)

# Check if submodules have been initialized
find_package(Git QUIET)
message(STATUS "Checking submodules")
if(GIT_FOUND AND EXISTS "${LGPROXY_TOP}/.git")
    option(GIT_SUBMODULE "Check submodules before build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Fetching submodules")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init repos/LookingGlass repos/LGMP repos/libtrf
                        WORKING_DIRECTORY ${LGPROXY_TOP}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT EXISTS "${LGPROXY_TOP}/repos")
    message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
endif()

get_filename_component(REPOS_TOP "${LGPROXY_TOP}/repos" ABSOLUTE)
get_filename_component(LG_TOP "${REPOS_TOP}/LookingGlass" ABSOLUTE)
list(APPEND CMAKE_MODULE_PATH "${LG_TOP}/cmake")
add_definitions(-DLP_LOG_USE_COLOR)
include(OptimizeForNative)

include_directories(
    "${PROJECT_SOURCE_DIR}/include"
    "${REPOS_TOP}/LGMP/lgmp/include"
    "${REPOS_TOP}/libtrf/libtrf/include"
    "${REPOS_TOP}/LookingGlass"
    "${REPOS_TOP}/LookingGlass/common/include"
)

add_subdirectory("${REPOS_TOP}/LGMP/lgmp" 
                "${CMAKE_BINARY_DIR}/libs/lgmp")
add_subdirectory("${REPOS_TOP}/libtrf/libtrf" 
                "${CMAKE_BINARY_DIR}/libs/libtrf")
add_subdirectory("${REPOS_TOP}/LookingGlass/common"
                "${CMAKE_BINARY_DIR}/libs/LookingGlass/common")   

add_compile_options(
"-Wall"
"-Werror"
"-Wfatal-errors"
"-ffast-math"
"-fdata-sections"
"-ffunction-sections"
"$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
# "-O0;-g3;-ggdb"
) 

file(COPY "${REPOS_TOP}/libtrf/libtrf/conf" DESTINATION "./source_build")
file(COPY "${REPOS_TOP}/libtrf/libtrf/conf" DESTINATION "./sink_build")

set(LP_COMMON
    common/src/lp_log.c
    common/src/lp_retrieve.c
    common/src/lp_trf_client.c
    common/src/lp_trf_server.c
    common/src/lp_types.c
    common/src/lp_write.c
    common/src/lp_msg.pb-c.c
    common/src/lp_msg.c
    common/src/lp_utils.c
)

set(SOURCE 
    ${LP_COMMON}
    source/lp_source.c
)
set(SINK 
    ${LP_COMMON}
    sink/lp_sink.c
)

add_executable(source ${SOURCE})
target_link_libraries(source trf lgmp m lg_common protobuf-c)
set_property(TARGET source PROPERTY C_STANDARD 11)

set_target_properties(source PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./source_build")

add_executable(sink ${SINK})

target_link_libraries(sink trf lgmp m lg_common protobuf-c)
set_property(TARGET sink PROPERTY C_STANDARD 11)

set_target_properties(sink PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./sink_build")       

target_include_directories(sink PUBLIC
                            "${LGPROXY_TOP}/sink"
                            "${LGPROXY_TOP}/repos/lgmp/lgmp"
                            "${LGPROXY_TOP}/repos/libtrf/libtrf"
                            "${LGPROXY_TOP}/repos/LookingGlass/common/include"
                            "${LGPROXY_TOP}/lgproxy/common/include")
target_include_directories(source PUBLIC 
                            "${LGPROXY_TOP}/source"
                            "${LGPROXY_TOP}/repos/lgmp/lgmp"
                            "${LGPROXY_TOP}/repos/libtrf/libtrf"
                            "${LGPROXY_TOP}/repos/LookingGlass/common/include"
                            "${LGPROXY_TOP}/lgproxy/common/include")


# Get Looking Glass version
execute_process(
    COMMAND sh -c "git submodule | grep -i LookingGlass | awk -F'[()]' '{print $2}'"
    WORKING_DIRECTORY "${LGPROXY_TOP}"
    OUTPUT_VARIABLE LG_GIT_REV
    ERROR_QUIET
)

if ("${LG_GIT_REV}" STREQUAL "")
	set(LG_GIT_REV "UNKNOWN")
endif()

string(STRIP "${LG_GIT_REV}" LGPROXY_LG_GIT_VER)
set(BUILD_VERSION "static const char * LG_BUILD_VERSION = \"${LGPROXY_LG_GIT_VER}\";")

string(TIMESTAMP YEAR "%Y")
set(HEADER "\
#define LG_VERSION_STR \"${LGPROXY_LG_GIT_VER}\"\n\
#define LG_CURRENT_YEAR ${YEAR}\n")

if(EXISTS "common/include/version.h")
	file(READ common/include/version.h LG_BUILD_VERSION_)
else()
	set(LG_BUILD_VERSION_ "")
endif()

if (NOT "${BUILD_VERSION}" STREQUAL "${LG_BUILD_VERSION_}")
	file(WRITE common/include/version.h "${BUILD_VERSION}\n")
endif()

# Get LG Proxy version
execute_process(
    COMMAND sh -c "git rev-parse --short HEAD"
    WORKING_DIRECTORY "${LGPROXY_TOP}"
    OUTPUT_VARIABLE LP_GIT_REV
    ERROR_QUIET
)

if ("${LP_GIT_REV}" STREQUAL "")
	set(LP_GIT_REV "UNKNOWN")
endif()

string(STRIP "${LP_GIT_REV}" PROXY_GIT_VER)
set(BUILD_VERSION "static const char * LP_BUILD_VERSION = \"${PROXY_GIT_VER}\";")

string(TIMESTAMP YEAR "%Y")
set(HEADER "\
#define LG_VERSION_STR \"${PROXY_GIT_VER}\"\n\
#define LG_CURRENT_YEAR ${YEAR}\n")

if(EXISTS "common/include/version.h")
	file(READ common/include/version.h LP_BUILD_VERSION_)
else()
	set(LP_BUILD_VERSION_ "")
endif()

if (NOT "${BUILD_VERSION}" STREQUAL "${LP_BUILD_VERSION_}")
	file(APPEND common/include/version.h "${BUILD_VERSION}")
endif()

feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)