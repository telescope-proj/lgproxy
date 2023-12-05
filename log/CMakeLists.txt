cmake_minimum_required(VERSION 3.5.0)
project(lgproxy_log LANGUAGES C)

set(SOURCES
    lp_log.c
)

add_library(lgproxy_log STATIC ${SOURCES})
set_property(TARGET lgproxy_log PROPERTY C_STANDARD 99)

target_include_directories(lgproxy_log PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)