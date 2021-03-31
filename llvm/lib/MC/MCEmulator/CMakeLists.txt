add_llvm_component_library(LLVMMCEmulator
  MCEmulator.cpp
  MCEmulatorState.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/MC/MCEmulator

  LINK_COMPONENTS
  MC
  Support
  )
