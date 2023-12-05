get_filename_component(PROJECT_TOP 
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    ABSOLUTE
)

get_filename_component(NETFR_TOP
    "${PROJECT_TOP}/repos/netfr"
    ABSOLUTE
)

get_filename_component(LG_TOP      
    "${PROJECT_TOP}/repos/LookingGlass" 
    ABSOLUTE
)

get_filename_component(LGMP_TOP    
    "${LG_TOP}/repos/LGMP"
    ABSOLUTE
)

include_directories(
    "${LG_TOP}"
    "${LGPROXY_TOP}/common/include"
    "${NETFR_TOP}/include"
    "${LG_TOP}/common/include"
    "${LGMP_TOP}/lgmp/include"
    "${TCM_TOP}/tcm/include"
)

add_subdirectory(
    "${LG_TOP}/common"
    "${CMAKE_BINARY_DIR}/libs/LookingGlass/common"
)

add_subdirectory(
    "${LGMP_TOP}/lgmp"
    "${CMAKE_BINARY_DIR}/libs/LGMP"
)

add_subdirectory(
    "${NETFR_TOP}/netfr"
    "${CMAKE_BINARY_DIR}/libs/netfr"
)

add_subdirectory(
    "${PROJECT_TOP}/log"
    "${CMAKE_BINARY_DIR}/libs/lgproxy_log"
)
