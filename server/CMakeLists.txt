cmake_minimum_required(VERSION 3.5)
project(lgproxy_server LANGUAGES C CXX)

get_filename_component(PROJECT_TOP
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    ABSOLUTE
)

get_filename_component(COMMON_DIR 
    "${PROJECT_TOP}/lgproxy/common" 
    ABSOLUTE
)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
    message(FATAL_ERROR
        "\n"
        "In-source builds are not supported\n"
        "See the LGProxy documentation for more details"
    )
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=nehalem")

add_compile_options(
    "-Wall"
    "-Wextra"
    "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
)

set(SOURCES
    lp_server.cpp
    lp_lgmp_client.cpp
)

add_subdirectory(
    "${COMMON_DIR}"
    "${CMAKE_BINARY_DIR}/libs/lgproxy_common"
)

add_executable(server ${SOURCES})
set_property(TARGET server PROPERTY C_STANDARD 11)
set_property(TARGET server PROPERTY CXX_STANDARD 11)
target_link_libraries(server lgproxy_common lgproxy_log)
target_include_directories(server PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${COMMON_DIR}/include"
    "${PROJECT_TOP}/repos/LookingGlass"
)