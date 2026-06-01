cmake_minimum_required(VERSION 3.20)

# ---- Language standard -----------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Coverage (inherits parent setting) ------------------------------
if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} --coverage -O0 -g -fprofile-arcs -ftest-coverage")
    set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE ON)
endif()

include(CMakePrintHelpers)
include(FetchContent)

# ---- GoogleTest ------------------------------------------------------
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# Prevent googletest from overriding our compiler/linker settings.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ---- tinyxml2 (required by config_parser_test.cpp) ------------------
FetchContent_Declare(
    tinyxml2
    GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
    GIT_TAG 9.0.0
)
FetchContent_MakeAvailable(tinyxml2)

# ---- Test executable -------------------------------------------------
set(TEST_NAME ${PROJECT_NAME}_tests)

add_executable(${TEST_NAME}
    hello_test.cpp
    config_parser_test.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        GTest::gtest_main
        GTest::gmock_main
        tinyxml2
)

target_include_directories(${TEST_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# ---- Test discovery --------------------------------------------------
include(GoogleTest)
gtest_discover_tests(${TEST_NAME}
    PROPERTIES TIMEOUT 60
)
