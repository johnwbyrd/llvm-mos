set(LLVM_LINK_COMPONENTS
  AllTargetsAsmParsers
  AllTargetsCodeGens
  AllTargetsDescs
  AllTargetsDisassemblers
  AllTargetsInfos
  Emulator
  MC
  MCParser
  Object
  Support
  TargetParser
)

add_llvm_tool(llvm-emu
  llvm-emu.cpp
)
