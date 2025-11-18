cmake_minimum_required(VERSION 3.10)

add_library(ehshell OBJECT)
target_sources(ehshell PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src/ehshell_core.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ehshell_builtin_commands.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/ehshell_escape_char.c"
)

target_include_directories(ehshell PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/include/")
target_link_libraries(ehshell PRIVATE eventhub)
