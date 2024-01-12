# From the Looking Glass repo (CheckSubmodule.cmake)

if (EXISTS "${LGPROXY_TOP}/.git" AND (
    (NOT EXISTS "${LGPROXY_TOP}/repos/LookingGlass/.git") OR 
    (NOT EXISTS "${LGPROXY_TOP}/repos/netfr/.git") OR
    (NOT EXISTS "${LGPROXY_TOP}/repos/imgui/.git")
))
    message(FATAL_ERROR "Submodules are not initialized. Run\n\tgit submodule update --init --recursive")
endif()

if (EXISTS "${LGPROXY_TOP}/.git" AND NOT DEFINED DEVELOPER)
    execute_process(
        COMMAND git submodule summary
        WORKING_DIRECTORY "${LGPROXY_TOP}"
        OUTPUT_VARIABLE SUBMODULE_SUMMARY
    )
    if (NOT "${SUBMODULE_SUMMARY}" STREQUAL "")
       message(FATAL_ERROR "Wrong submodule version checked out. Run\n\tgit submodule update --init --recursive")
    endif()
endif()