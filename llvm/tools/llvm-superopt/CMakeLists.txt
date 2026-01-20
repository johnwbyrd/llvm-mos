set(LLVM_LINK_COMPONENTS
  AllTargetsDescs
  AllTargetsInfos
  MC
  MOSDisassembler
  MOSSuperopt
  Support
  TargetParser
  )

add_llvm_tool(llvm-superopt
  llvm-superopt.cpp
  )
