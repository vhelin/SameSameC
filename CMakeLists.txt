
cmake_minimum_required (VERSION 2.8.12...3.5 FATAL_ERROR)
project (SameSameC C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/binaries/")

if(WIN32)
    add_definitions(-DWIN32)
    add_definitions(-DMSDOS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX)
    add_definitions(-DUNIX)
    link_libraries(m)
endif()

add_subdirectory (compiler)
add_subdirectory (linker)
