cmake_minimum_required(VERSION 3.10)

add_library(ehshell OBJECT)
target_sources(ehshell PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/ehshell_core.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ehshell_builtin_commands.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ehshell_escape_char.c"
)

target_include_directories(ehshell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/include/")
target_link_libraries(ehshell PRIVATE eventhub)

if(NOT CONFIG_PACKAGE_EHSHELL_BUILTIN_NONE)
    add_library(ehshell_builtin OBJECT)
    if(CONFIG_PACKAGE_EHSHELL_BUILTIN_SEGGER_RTT)
        target_sources(ehshell_builtin PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/port/segger_rtt_shell.c"
        )
    endif()
    target_link_libraries(ehshell_builtin PRIVATE ehshell segger-rtt eventhub )
endif()