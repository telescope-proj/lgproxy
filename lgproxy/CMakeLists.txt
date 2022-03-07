cmake_minimum_required(VERSION 3.0.0)
project(lgproxy LANGUAGES C)

get_filename_component(PROJECT_TOP "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)

add_definitions(-DLP_LOG_USE_COLOR)

include_directories(
    "${PROJECT_SOURCE_DIR}/include"
    "${PROJECT_TOP}/repos/LGMP/lgmp/include"
    "${PROJECT_TOP}/repos/libtrf/libtrf/include"
    "${PROJECT_TOP}/repos/LookingGlass/common/include"
)

add_subdirectory("${PROJECT_TOP}/repos/LGMP/lgmp" 
                "${CMAKE_BINARY_DIR}/libs/lgmp")
add_subdirectory("${PROJECT_TOP}/repos/libtrf/libtrf" 
                "${CMAKE_BINARY_DIR}/libs/libtrf")
# add_subdirectory("${PROJECT_TOP}/repos/LookingGlass/common"
#                  "${CMAKE_BINARY_DIR}/libs/LookingGlass/common")
file(COPY "${PROJECT_TOP}/repos/LookingGlass/module/kvmfr.h"
     DESTINATION "${CMAKE_BINARY_DIR}/libs/LookingGlass/common/include/module/kvmfr.h")

set(LP_COMMON
    common/src/lp_log.c
    common/src/lp_retrieve.c
    common/src/lp_trf_client.c
    common/src/lp_trf_server.c
    common/src/lp_types.c
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
target_link_libraries(source trf lgmp m)
set_property(TARGET source PROPERTY C_STANDARD 99)

set_target_properties(source PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./build")

add_executable(sink ${SINK})

target_link_libraries(sink trf lgmp m)
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

