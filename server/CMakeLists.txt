cmake_minimum_required(VERSION 3.5)
project(lgproxy_server LANGUAGES C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-sanitize=alignment -march=nehalem")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fno-sanitize=alignment -march=nehalem")

get_filename_component(PROJECT_TOP
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    ABSOLUTE
)

get_filename_component(COMMON_DIR 
    "${PROJECT_TOP}/common" 
    ABSOLUTE
)

get_filename_component(IMGUI_TOP
    "${PROJECT_TOP}/repos/imgui"
    ABSOLUTE
)

if("${PROJECT_SOURCE_DIR}" STREQUAL "${PROJECT_BINARY_DIR}")
    message(FATAL_ERROR
        "\n"
        "In-source builds are not supported\n"
        "See the LGProxy documentation for more details"
    )
endif()



set(IMGUI_SOURCES
    "${IMGUI_TOP}/imgui.cpp"
    "${IMGUI_TOP}/imgui_draw.cpp"
    "${IMGUI_TOP}/imgui_demo.cpp"
    "${IMGUI_TOP}/imgui_widgets.cpp"
    "${IMGUI_TOP}/imgui_tables.cpp"
    "${IMGUI_TOP}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_TOP}/backends/imgui_impl_opengl3.cpp"
)

set(SOURCES
    "${IMGUI_SOURCES}"    
    lgmp_client.cpp
    common.cpp
    cursor.cpp
    frame.cpp
    main.cpp
    profiler.cpp
)

add_subdirectory(
    "${COMMON_DIR}"
    "${CMAKE_BINARY_DIR}/libs/lgproxy_common"
)

add_executable(server ${SOURCES})
set_property(TARGET server PROPERTY C_STANDARD 11)
set_property(TARGET server PROPERTY CXX_STANDARD 11)
target_link_libraries(server lgproxy_common lgproxy_log glfw GL)
target_include_directories(server PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${COMMON_DIR}/include"
    "${PROJECT_TOP}/repos/LookingGlass"
    "${IMGUI_TOP}"
    "${IMGUI_TOP}/backends"
)

target_compile_options(
    server PRIVATE
    "-Wall"
    "-Wextra"
    "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb3>"
)
