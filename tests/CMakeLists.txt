cmake_minimum_required(VERSION 3.21)
project(WaveHelperTests CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mirror-math-test MirrorMathTest.cpp)
if (MSVC)
    target_compile_options(mirror-math-test PRIVATE /W4 /WX)
else()
    target_compile_options(mirror-math-test PRIVATE -Wall -Wextra -Werror)
endif()

enable_testing()
add_test(NAME mirror-math COMMAND mirror-math-test)
