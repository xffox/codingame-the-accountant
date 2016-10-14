cmake_minimum_required(VERSION 2.8)

project(accountant)

include(CTest)

set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")

if(UNIX)
    set(STANDARD_FLAGS "-ansi")
    set(WARN_FLAGS "-Wall -Wextra -pedantic -std=c++11")
    add_definitions("${STANDARD_FLAGS}" "${WARN_FLAGS}")
elseif(WIN32)
    add_definitions("-DNOMINMAX")
endif()

file(GLOB MAIN_SRCS "*.cpp")
file(GLOB MAIN_HDRS "*.h")

add_executable(run ${MAIN_SRCS})
add_custom_command(OUTPUT "out.cpp"
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/combine ${CMAKE_CURRENT_SOURCE_DIR}/main.cb ${CMAKE_CURRENT_BINARY_DIR}/out.cpp
    DEPENDS ${MAIN_SRCS} ${MAIN_HDRS} "${CMAKE_CURRENT_SOURCE_DIR}/main.cb"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_executable(run_out "out.cpp")

add_subdirectory(test)
