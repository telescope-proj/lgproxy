cmake_minimum_required(VERSION 3.0.0)
project(lgproxy LANGUAGES C)

get_filename_component(PROJECT_TOP "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)

add_definitions(-DLP_LOG_USE_COLOR)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx")

# Check if submodules have been initialized
find_package(Git QUIET)
message(STATUS "Checking Submodules")
if(GIT_FOUND AND EXISTS "${PROJECT_TOP}/.git")
    option(GIT_SUBMODULE "Check submodules before build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Fetching Libtrf")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init repos/LookingGlass repos/LGMP repos/libtrf
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

include_directories(
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_TOP}/repos/LGMP/lgmp/include"
    "${PROJECT_TOP}/repos/libtrf/libtrf/include"
    "${PROJECT_TOP}/repos/LookingGlass"
    "${PROJECT_TOP}/repos/LookingGlass/common/include"
)

add_subdirectory("${PROJECT_TOP}/repos/LGMP/lgmp" 
                "${CMAKE_BINARY_DIR}/libs/lgmp")
add_subdirectory("${PROJECT_TOP}/repos/libtrf/libtrf" 
                "${CMAKE_BINARY_DIR}/libs/libtrf")
add_subdirectory("${PROJECT_TOP}/repos/LookingGlass/common"
                "${CMAKE_BINARY_DIR}/libs/LookingGlass/common")   

add_compile_options(
"-Wall"
"-Werror"
"-Wfatal-errors"
"-ffast-math"
"-fdata-sections"
"-ffunction-sections"
#"$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
"-O0;-g3;-ggdb"
) 

file(COPY "${PROJECT_TOP}/repos/libtrf/libtrf/conf" DESTINATION "./source_build")
file(COPY "${PROJECT_TOP}/repos/libtrf/libtrf/conf" DESTINATION "./sink_build")

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
set_property(TARGET source PROPERTY C_STANDARD 99)

set_target_properties(source PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./source_build")

add_executable(sink ${SINK})

target_link_libraries(sink trf lgmp m lg_common protobuf-c)
set_property(TARGET sink PROPERTY C_STANDARD 99)

set_target_properties(sink PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./sink_build")       

target_include_directories(sink PUBLIC
                            "${PROJECT_TOP}/sink"
                            "${PROJECT_TOP}/repos/lgmp/lgmp"
                            "${PROJECT_TOP}/repos/libtrf/libtrf"
                            "${PROJECT_TOP}/repos/LookingGlass/common/include"
                            "${PROJECT_TOP}/lgproxy/common/include")
target_include_directories(source PUBLIC 
                            "${PROJECT_TOP}/source"
                            "${PROJECT_TOP}/repos/lgmp/lgmp"
                            "${PROJECT_TOP}/repos/libtrf/libtrf"
                            "${PROJECT_TOP}/repos/LookingGlass/common/include"
                            "${PROJECT_TOP}/lgproxy/common/include")


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
    WORKING_DIRECTORY "${PROJECT_TOP}"
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