# Copied almost verbatim from https://github.com/llvm/llvm-project/blob/main/clang/examples/PrintFunctionNames/CMakeLists.txt


cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(redundant_scope_checker VERSION 1.0.0 LANGUAGES CXX)

find_package(LLVM 11 REQUIRED CONFIG)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_llvm_library(RedundantScopeChecker MODULE RedundantScopeChecker.cc PLUGIN_TOOL clang)

if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS
    Support
  )
  clang_target_link_libraries(RedundantScopeChecker PRIVATE
    clangAST
    clangBasic
    clangFrontend
    clangLex
    )
endif()
