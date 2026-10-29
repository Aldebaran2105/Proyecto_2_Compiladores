cmake_minimum_required(VERSION 3.16)
project(robotlang CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Preserve the original scanner-only executable. Semantic integration awaits AST.
add_executable(robotc src/main.cpp src/scanner.cpp src/token.cpp)

add_library(robotlang_semantic src/semantic.cpp src/ir.cpp)
target_include_directories(robotlang_semantic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(semantic_contract_demo examples/semantic_contract_demo.cpp src/scanner.cpp)
target_link_libraries(semantic_contract_demo PRIVATE robotlang_semantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(semantic_ir_tests tests/semantic_ir_tests.cpp src/scanner.cpp)
    target_link_libraries(semantic_ir_tests PRIVATE robotlang_semantic)
    add_test(NAME semantic_ir_tests COMMAND semantic_ir_tests)
    add_test(NAME semantic_contract_demo COMMAND semantic_contract_demo)
    add_test(NAME original_scanner_square
             COMMAND robotc ${CMAKE_CURRENT_SOURCE_DIR}/examples/square.robot --tokens)
endif()
