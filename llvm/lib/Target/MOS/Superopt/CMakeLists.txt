add_llvm_component_library(LLVMMOSSuperopt
  Enumerator.cpp
  MOSSuperopt.cpp
  Verifier.cpp

  LINK_COMPONENTS
  MC
  MOSDesc
  Support

  DEPENDS
  MOSCommonTableGen
)
