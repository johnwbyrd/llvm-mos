set(LLVM_LINK_COMPONENTS
  AllTargetsAsmParsers
  AllTargetsDescs
  AllTargetsDisassemblers
  AllTargetsEmulators
  AllTargetsInfos
  MC
  MCEmulator
  MCParser
  Support
  )

add_llvm_tool(llvm-emu
  llvm-emu.cpp
  Disassembler.cpp
  )
