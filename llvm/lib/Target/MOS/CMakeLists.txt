add_llvm_component_group(MOS)

set(LLVM_TARGET_DEFINITIONS MOS.td)

tablegen(LLVM MOSGenAsmMatcher.inc -gen-asm-matcher)
tablegen(LLVM MOSGenAsmWriter.inc -gen-asm-writer)
tablegen(LLVM MOSGenCallingConv.inc -gen-callingconv)
tablegen(LLVM MOSGenDAGISel.inc -gen-dag-isel)
tablegen(LLVM MOSGenDisassemblerTables.inc -gen-disassembler)
# Generate emulator with SAIL semantics
# The IR file is checked into the repo - regenerate with 'ninja mos-sail-ir'
set(MOS_SAIL_IR ${CMAKE_CURRENT_SOURCE_DIR}/Sail/mos6502.ir)
tablegen(LLVM MOSGenEmulator.inc -gen-emulator -sail-ir=${MOS_SAIL_IR} DEPENDS ${MOS_SAIL_IR})
tablegen(LLVM MOSGenGICombiner.inc -gen-global-isel-combiner -combiners="MOSCombiner")
tablegen(LLVM MOSGenGlobalISel.inc -gen-global-isel)
tablegen(LLVM MOSGenInstrInfo.inc -gen-instr-info)
tablegen(LLVM MOSGenMCCodeEmitter.inc -gen-emitter)
tablegen(LLVM MOSGenMCPseudoLowering.inc -gen-pseudo-lowering)
tablegen(LLVM MOSGenRegisterBank.inc -gen-register-bank)
tablegen(LLVM MOSGenRegisterInfo.inc -gen-register-info)
tablegen(LLVM MOSGenSearchableTables.inc -gen-searchable-tables)
tablegen(LLVM MOSGenSubtargetInfo.inc -gen-subtarget)

add_public_tablegen_target(MOSCommonTableGen)

add_llvm_target(MOSCodeGen
  MOSAsmPrinter.cpp
  MOSCallGraphUtils.cpp
  MOSCallLowering.cpp
  MOSCallingConv.cpp
  MOSCombiner.cpp
  MOSCopyOpt.cpp
  MOSFrameLowering.cpp
  MOSISelLowering.cpp
  MOSIndexIV.cpp
  MOSInlineAsmLowering.cpp
  MOSInsertCopies.cpp
  MOSInstrCost.cpp
  MOSInstrInfo.cpp
  MOSInstructionSelector.cpp
  MOSInternalize.cpp
  MOSLateOptimization.cpp
  MOSLegalizerInfo.cpp
  MOSLowerSelect.cpp
  MOSMCInstLower.cpp
  MOSMachineScheduler.cpp
  MOSNonReentrant.cpp
  MOSPostRAScavenging.cpp
  MOSRegisterBankInfo.cpp
  MOSRegisterInfo.cpp
  MOSShiftRotateChain.cpp
  MOSStaticStackAlloc.cpp
  MOSSubtarget.cpp
  MOSTargetMachine.cpp
  MOSTargetObjectFile.cpp
  MOSZeroPageAlloc.cpp

  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
  Analysis
  AsmPrinter
  BinaryFormat
  CodeGen
  CodeGenTypes
  Core
  Emulator
  GlobalISel
  IPO
  InstCombine
  LTO
  MC
  MOSDesc
  MOSInfo
  Passes
  Scalar
  SelectionDAG
  Support
  Target
  TargetParser
  TransformUtils

  ADD_TO_COMPONENT
  MOS
)

add_subdirectory(AsmParser)
add_subdirectory(Disassembler)
add_subdirectory(MCTargetDesc)
add_subdirectory(TargetInfo)

# Optional target to regenerate SAIL IR from source
# Requires sail and isla-sail plugin (from https://github.com/rems-project/isla)
# Usage: ninja mos-sail-ir
set(MOS_SAIL_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/Sail/mos6502.sail)
set(MOS_SAIL_IR ${CMAKE_CURRENT_SOURCE_DIR}/Sail/mos6502.ir)
set(ISLA_PLUGIN "$ENV{HOME}/git/isla/isla-sail/_build/default/sail_plugin_isla.cmxs")

add_custom_target(mos-sail-ir
  COMMAND sail -plugin ${ISLA_PLUGIN} -isla -o ${CMAKE_CURRENT_SOURCE_DIR}/Sail/mos6502 ${MOS_SAIL_SOURCE}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Sail
  COMMENT "Regenerating MOS SAIL IR from mos6502.sail"
  SOURCES ${MOS_SAIL_SOURCE}
)
set_target_properties(mos-sail-ir PROPERTIES EXCLUDE_FROM_ALL TRUE)
