cmake_minimum_required(VERSION 3.0.0)
project(lgproxy)

include_directories(include)
set(SERVER test-server.c)
set(CLIENT test-client.c)

get_filename_component(PROJECT_TOP "${PROJECT_SOURCE_DIR}/.." ABSOLUTE)
add_subdirectory("${PROJECT_TOP}/lgmp" "${CMAKE_BINARY_DIR}/lgmp")


add_executable(test-server ${SERVER})
target_link_libraries(test-server lgmp)
set_property(TARGET test-server PROPERTY C_STANDARD 99)
add_executable(test-client ${CLIENT})
target_link_libraries(test-client lgmp)
set_property(TARGET test-client PROPERTY C_STANDARD 99)