cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
project(ffmpeg_node_editor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(gmp REQUIRED IMPORTED_TARGET gmp)
pkg_check_modules(gmpxx REQUIRED IMPORTED_TARGET gmpxx)

add_library(core INTERFACE)
target_include_directories(core INTERFACE "${CMAKE_SOURCE_DIR}")
target_link_libraries(core INTERFACE PkgConfig::gmpxx PkgConfig::gmp)
target_compile_options(core INTERFACE -mavx512bw)
target_link_options(core INTERFACE -fsanitize=address,undefined)

add_executable(bfc compiler.cpp)
target_compile_options(bfc PRIVATE -g)
add_executable(bfi interpreter.cpp)
target_compile_options(bfi PRIVATE -Ofast)

target_link_libraries(bfc PRIVATE core)
target_link_libraries(bfi PRIVATE core)

find_package(LLVM CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
add_definitions(${LLVM_DEFINITIONS})
llvm_map_components_to_libnames(llvm_libs Core Support x86asmparser x86codegen
                                passes)

target_include_directories(bfc PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_libraries(bfc PRIVATE ${llvm_libs})
