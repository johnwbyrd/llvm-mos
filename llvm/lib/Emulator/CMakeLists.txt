add_subdirectory(Semihost)

add_llvm_component_library(LLVMEmulator
  Context.cpp
  Semihost.cpp
  System.cpp
  Trace.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Emulator

  LINK_COMPONENTS
  EmulatorSemihost
  MC
  Support
  )
