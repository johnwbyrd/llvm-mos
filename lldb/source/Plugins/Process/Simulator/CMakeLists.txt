add_lldb_library(lldbPluginProcessSimulator PLUGIN
  ProcessSimulator.cpp
  ThreadSimulator.cpp
  RegisterContextSimulator.cpp

  LINK_COMPONENTS
    MC
    Object
    Support

  LINK_LIBS
    lldbCore
    lldbTarget
    lldbSymbol
    lldbUtility
    lldbPluginProcessUtility
    LLVMEmulator
)
