cmake_minimum_required(VERSION 3.16)
project(robotlang CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

file(GLOB SOURCES src/*.cpp)
add_executable(robotc ${SOURCES})
