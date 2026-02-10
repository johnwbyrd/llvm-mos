add_llvm_component_library(LLVMEmulatorSemihost
  Backend.cpp
  FileBackend.cpp
  FileDescTable.cpp
  PathValidator.cpp
  RiffCodec.cpp
  SecureBackend.cpp

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Emulator/Semihost

  LINK_COMPONENTS
  Support
  )
