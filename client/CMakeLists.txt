cmake_minimum_required(VERSION 3.5)
project(lgproxy_client LANGUAGES C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-sanitize=alignment -fsanitize=undefined")

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
        "In-source builds are not allowed\n"
        "See the LGProxy documentation for more details"
    )
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=nehalem")


set(SOURCES
    lp_client.cpp
    lp_lgmp_host.cpp
)

add_subdirectory(
    "${COMMON_DIR}"
    "${CMAKE_BINARY_DIR}/libs/lgproxy_common"
)

add_executable(client ${SOURCES})
set_property(TARGET client PROPERTY C_STANDARD 11)
set_property(TARGET client PROPERTY CXX_STANDARD 11)
target_link_libraries(client lgproxy_common lgproxy_log)
target_include_directories(client SYSTEM PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${COMMON_DIR}/include"
    "${PROJECT_TOP}/repos/LookingGlass"
)
target_compile_options(
    client PRIVATE
    "-Wall"
    "-Wextra"
    "-fno-sanitize=alignment"
    "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb>"
)
