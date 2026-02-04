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
    list(APPEND EHSHELL_BUILTIN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/port/ehshell_builtin.c" )
    list(APPEND EHSHELL_BUILTIN_LINK_LIBRARIES ehshell)
    list(APPEND EHSHELL_BUILTIN_LINK_LIBRARIES eventhub)

    if(CONFIG_PACKAGE_EHSHELL_BUILTIN_SEGGER_RTT)
        list(APPEND EHSHELL_BUILTIN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/port/segger_rtt_shell.c" )
        list(APPEND EHSHELL_BUILTIN_LINK_LIBRARIES segger-rtt)
    endif()

    if(CONFIG_PACKAGE_EHSHELL_BUILTIN_TELNET_SERVER)
        list(APPEND EHSHELL_BUILTIN_SOURCES "${CMAKE_CURRENT_LIST_DIR}/port/telnet_server_shell.c" )
        list(APPEND EHSHELL_BUILTIN_LINK_LIBRARIES ehip)
    endif()

    target_sources(ehshell_builtin PRIVATE
        ${EHSHELL_BUILTIN_SOURCES}
    )
    target_link_libraries(ehshell_builtin PRIVATE ${EHSHELL_BUILTIN_LINK_LIBRARIES} )
endif()