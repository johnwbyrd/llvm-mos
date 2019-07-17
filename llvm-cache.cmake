cmake_minimum_required (VERSION 2.6)
set(LLVM_ENABLE_PROJECTS "clang;clang-tools-extra;libcxx;libcxxabi;lldb;compiler-rt;lld" CACHE STRING "")
set(LLVM_EXPERIMENTAL_TARGETS_TO_BUILD "AVR;MOS" CACHE STRING "")
