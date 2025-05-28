add_lldb_library(lldbPluginArchitectureMOS PLUGIN
  ABISysV_mos.cpp
  MOSGDBRemoteRegisterContext.cpp
  LLDBMOSLog.cpp

  LINK_LIBS
    lldbCore
    lldbSymbol
    lldbTarget
    lldbValueObject
    lldbUtility
  LINK_COMPONENTS
    Support
    TargetParser
  ) 