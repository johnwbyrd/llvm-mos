add_llvm_component_library(LLVMEmulatorSemihost
  zbc_ansi_common.c
  zbc_ansi_console.c
  zbc_ansi_insecure.c
  zbc_ansi_secure.c
  zbc_backend_dummy.c
  zbc_host.c
  zbc_opcode_table.c
  zbc_riff.c

  ADDITIONAL_HEADER_DIRS
  ${LLVM_MAIN_INCLUDE_DIR}/llvm/Emulator/Semihost

  LINK_COMPONENTS
  Support
  )
