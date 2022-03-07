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
add_subdirectory("${PROJECT_TOP}/repos/LookingGlass/common")

set(HOST host/host.c)
set(CLIENT client/client.c)

add_executable(host ${HOST})
target_link_libraries(host trf lgmp)
set_property(TARGET host PROPERTY C_STANDARD 99)

set_target_properties(host PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./build")

add_executable(client ${CLIENT})

target_link_libraries(client trf lgmp)
set_property(TARGET client PROPERTY C_STANDARD 99)

set_target_properties(client PROPERTIES RUNTIME_OUTPUT_DIRECTORY "./build")

target_include_directories(lgproxy
                            INTERFACE
                                "./common/include"
                            PRIVATE
                                src
)               

target_include_directories(client INTERFACE
                            "${PROJECT_TOP}/repos/lgmp/lgmp"
                            "${PROJECT_TOP}/repos/libtrf/libtrf"
                            "${PROJECT_TOP}/repos/LookingGlass/common/include")
target_include_directories(host INTERFACE 
                            "${PROJECT_TOP}/repos/lgmp/lgmp"
                            "${PROJECT_TOP}/repos/libtrf/libtrf"
                            "${PROJECT_TOP}/repos/LookingGlass/common/include")

