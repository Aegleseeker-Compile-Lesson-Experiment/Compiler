cmake_minimum_required(VERSION 3.16)
project(toyc_compiler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(compiler
  src/main.cpp
  src/token.cpp
  src/lexer.cpp
)
target_include_directories(compiler PRIVATE src)