//===-- MOSMCTargetDesc.h - MOS Target Descriptions -------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file provides MOS specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MOS_MCTARGET_DESC_H
#define LLVM_MOS_MCTARGET_DESC_H

#include "llvm/ADT/Sequence.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/Support/DataTypes.h"

#include <memory>

namespace llvm {

class FeatureBitset;
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCRegisterInfo;
class MCSubtargetInfo;
class MCTargetOptions;
class StringRef;
class Target;
class Triple;
class raw_pwrite_stream;

Target &getTheMOSTarget();

MCInstrInfo *createMOSMCInstrInfo();

/// Creates a machine code emitter for MOS.
MCCodeEmitter *createMOSMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx);

/// Creates an assembly backend for MOS.
MCAsmBackend *createMOSAsmBackend(const Target &T, const MCSubtargetInfo &STI,
                                  const MCRegisterInfo &MRI,
                                  const llvm::MCTargetOptions &TO);

/// Creates an ELF object writer for MOS.
std::unique_ptr<MCObjectTargetWriter> createMOSELFObjectWriter(uint8_t OSABI);

} // end namespace llvm

#define GET_REGINFO_ENUM
#include "MOSGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "MOSGenInstrInfo.inc"

#define GET_SUBTARGETINFO_ENUM
#include "MOSGenSubtargetInfo.inc"

namespace llvm {
template <> struct enum_iteration_traits<decltype(MOS::NoRegister)> {
  static constexpr bool is_iterable = true;
};

namespace MOSOp {

enum OperandType : unsigned {
  OPERAND_IMM8 = MCOI::OPERAND_FIRST_TARGET,
  OPERAND_ADDR8,
  OPERAND_ADDR16,
  OPERAND_IMM16,
  OPERAND_IMM3,
  OPERAND_ADDR24,
  OPERAND_IMM24,
  OPERAND_ADDR13,
  OPERAND_IMM4
};

} // namespace MOSOp

namespace MOS {

//===----------------------------------------------------------------------===//
// TSFlags Layout
//===----------------------------------------------------------------------===//
// The TSFlags field in MCInstrDesc encodes instruction metadata:
//
// Bits 0-3:   65816 M/X flag requirements
// Bits 4-11:  Base cycle count (8 bits, 0-255)
// Bits 12-15: Page cross penalty cycles (4 bits, 0-15)
// Bits 16-29: Flag effects (2 bits each × 7 flags)
//
// Flag effect encoding:
//   0 = unaffected
//   1 = modified (value depends on operation result)
//   2 = always set to 1
//   3 = always cleared to 0

// Legacy M/X flag bits (65816)
enum TSFlag {
  TSFlagMLow = (1 << 0),
  TSFlagMHigh = (1 << 1),
  TSFlagXLow = (1 << 2),
  TSFlagXHigh = (1 << 3)
};

// TSFlags bit positions and masks
namespace TSFlagBits {
  // Cycle counts
  constexpr unsigned CyclesShift = 4;
  constexpr uint64_t CyclesMask = 0xFFULL << CyclesShift;  // bits 4-11

  constexpr unsigned PageCrossShift = 12;
  constexpr uint64_t PageCrossMask = 0xFULL << PageCrossShift;  // bits 12-15

  // Flag effects (2 bits each)
  constexpr unsigned FlagNShift = 16;
  constexpr unsigned FlagVShift = 18;
  constexpr unsigned FlagBShift = 20;
  constexpr unsigned FlagDShift = 22;
  constexpr unsigned FlagIShift = 24;
  constexpr unsigned FlagZShift = 26;
  constexpr unsigned FlagCShift = 28;
  constexpr uint64_t FlagMask = 0x3ULL;  // 2-bit mask for each flag
} // namespace TSFlagBits

// Flag effect values
enum FlagEffect : unsigned {
  FlagUnaffected = 0,
  FlagModified = 1,
  FlagSet = 2,
  FlagCleared = 3
};

// Accessor functions for TSFlags
inline unsigned getCycles(uint64_t TSFlags) {
  return (TSFlags & TSFlagBits::CyclesMask) >> TSFlagBits::CyclesShift;
}

inline unsigned getPageCrossCycles(uint64_t TSFlags) {
  return (TSFlags & TSFlagBits::PageCrossMask) >> TSFlagBits::PageCrossShift;
}

inline FlagEffect getFlagN(uint64_t TSFlags) {
  return static_cast<FlagEffect>(
      (TSFlags >> TSFlagBits::FlagNShift) & TSFlagBits::FlagMask);
}

inline FlagEffect getFlagV(uint64_t TSFlags) {
  return static_cast<FlagEffect>(
      (TSFlags >> TSFlagBits::FlagVShift) & TSFlagBits::FlagMask);
}

inline FlagEffect getFlagB(uint64_t TSFlags) {
  return static_cast<FlagEffect>(
      (TSFlags >> TSFlagBits::FlagBShift) & TSFlagBits::FlagMask);
}

inline FlagEffect getFlagD(uint64_t TSFlags) {
  return static_cast<FlagEffect>(
      (TSFlags >> TSFlagBits::FlagDShift) & TSFlagBits::FlagMask);
}

inline FlagEffect getFlagI(uint64_t TSFlags) {
  return static_cast<FlagEffect>(
      (TSFlags >> TSFlagBits::FlagIShift) & TSFlagBits::FlagMask);
}

inline FlagEffect getFlagZ(uint64_t TSFlags) {
  return static_cast<FlagEffect>(
      (TSFlags >> TSFlagBits::FlagZShift) & TSFlagBits::FlagMask);
}

inline FlagEffect getFlagC(uint64_t TSFlags) {
  return static_cast<FlagEffect>(
      (TSFlags >> TSFlagBits::FlagCShift) & TSFlagBits::FlagMask);
}

// Helper to check if instruction modifies any flags
inline bool modifiesFlags(uint64_t TSFlags) {
  constexpr uint64_t AllFlagsMask =
      (TSFlagBits::FlagMask << TSFlagBits::FlagNShift) |
      (TSFlagBits::FlagMask << TSFlagBits::FlagVShift) |
      (TSFlagBits::FlagMask << TSFlagBits::FlagBShift) |
      (TSFlagBits::FlagMask << TSFlagBits::FlagDShift) |
      (TSFlagBits::FlagMask << TSFlagBits::FlagIShift) |
      (TSFlagBits::FlagMask << TSFlagBits::FlagZShift) |
      (TSFlagBits::FlagMask << TSFlagBits::FlagCShift);
  return (TSFlags & AllFlagsMask) != 0;
}

bool isZeroPageSectionName(StringRef Name);
} // namespace MOS
} // namespace llvm

#endif // LLVM_MOS_MCTARGET_DESC_H
