cmake_minimum_required(VERSION 2.8)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/test")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
set(ACCOUNTANT_TEST_NAME accountant_bench)
set(ACCOUNTANT_PERF_NAME accountant_perf)

include_directories("${CMAKE_SOURCE_DIR}")

set(ACCOUNTANT_TEST_SRCS
    "bench.cpp"
    "${CMAKE_SOURCE_DIR}/game.cpp"
    "${CMAKE_SOURCE_DIR}/logic.cpp"
    "${CMAKE_SOURCE_DIR}/optimizer.cpp"
    )

set(ACCOUNTANT_PERF_SRCS
    "perf.cpp"
    "${CMAKE_SOURCE_DIR}/game.cpp"
    "${CMAKE_SOURCE_DIR}/logic.cpp"
    "${CMAKE_SOURCE_DIR}/optimizer.cpp"
    )

add_executable(${ACCOUNTANT_TEST_NAME} ${ACCOUNTANT_TEST_SRCS})
add_executable(${ACCOUNTANT_PERF_NAME} ${ACCOUNTANT_PERF_SRCS})

add_test(NAME AccountantBench COMMAND ${ACCOUNTANT_TEST_NAME})
