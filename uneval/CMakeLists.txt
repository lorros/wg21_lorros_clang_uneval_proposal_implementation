cmake_minimum_required(VERSION 3.15)
project(uneval_clang_tidy_plugin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Explicitly set include and lib paths for system clang/llvm
include_directories(/usr/lib/llvm-14/include /usr/include/clang /usr/include/llvm)

link_directories(/lib/x86_64-linux-gnu)

add_subdirectory(src)
