cmake_minimum_required(VERSION 3.0.0)
project(lgproxy LANGUAGES C)

get_filename_component(PROJECT_TOP "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)

add_definitions(-DLP_LOG_USE_COLOR)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mavx")

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

execute_process(
    COMMAND bash -c "cp -r ${PROJECT_TOP}/repos/libtrf/libtrf/conf ./build"
)


set(LP_COMMON
    common/src/lp_log.c
    common/src/lp_retrieve.c
    common/src/lp_trf_client.c
    common/src/lp_trf_server.c
    common/src/lp_types.c
    common/src/lp_write.c
    common/src/lp_msg.pb-c.c
    common/src/lp_msg.c
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

set_target_properties(source PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./build")

add_executable(sink ${SINK})

target_link_libraries(sink trf lgmp m lg_common protobuf-c)
set_property(TARGET sink PROPERTY C_STANDARD 99)

set_target_properties(sink PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./build")       

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

execute_process(
    COMMAND sh -c "git submodule | grep -i LookingGlass | awk -F'[()]' '{print $2}'"
    WORKING_DIRECTORY "${PROJECT_TOP}"
    OUTPUT_VARIABLE LGPROXY_LG_GIT_REV
    ERROR_QUIET
)

if ("${LGPROXY_LG_GIT_REV}" STREQUAL "")
	set(LGPROXY_LG_GIT_REV "UNKNOWN")
endif()


string(STRIP "${LGPROXY_LG_GIT_REV}" LGPROXY_LG_GIT_VER)
set(BUILD_VERSION "static const char * LP_BUILD_VERSION = \"${LGPROXY_LG_GIT_VER}\";")

string(TIMESTAMP YEAR "%Y")
set(HEADER "\
#define LG_VERSION_STR \"${LGPROXY_LG_GIT_VER}\"\n\
#define LG_CURRENT_YEAR ${YEAR}\n")

if(EXISTS "common/include/version.h")
	file(READ common/include/version.h BUILD_VERSION_)
else()
	set(BUILD_VERSION_ "")
endif()

if (NOT "${BUILD_VERSION}" STREQUAL "${BUILD_VERSION_}")
	file(WRITE common/include/version.h "${BUILD_VERSION}")
endif()
