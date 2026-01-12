add_llvm_component_library(LLVMEmulator
  Context.cpp
  Trace.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Emulator

  LINK_COMPONENTS
  MC
  Support
  )
