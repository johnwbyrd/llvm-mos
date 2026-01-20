//===-- Superopt.h - MOS Superoptimizer Framework ---------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the superoptimizer framework for MOS targets.
// The design is intended to be generalizable to other LLVM targets.
//
// Architecture:
//   - Enumerator: Generates candidate instruction sequences using MCInst
//   - CostModel: Evaluates cost (bytes, cycles) using MCInstrDesc/TSFlags
//   - Verifier: Checks equivalence (pluggable - Isla/SMT or exhaustive testing)
//   - SearchStrategy: Controls enumeration order and pruning
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MOS_SUPEROPT_SUPEROPT_H
#define LLVM_LIB_TARGET_MOS_SUPEROPT_SUPEROPT_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include <functional>
#include <memory>

namespace llvm {
namespace superopt {

//===----------------------------------------------------------------------===//
// Cost Model (target-agnostic interface, target-specific implementation)
//===----------------------------------------------------------------------===//

/// Cost of an instruction sequence.
struct Cost {
  unsigned Bytes = 0;
  unsigned Cycles = 0;

  Cost() = default;
  Cost(unsigned B, unsigned C) : Bytes(B), Cycles(C) {}

  bool operator<(const Cost &Other) const {
    // Primary: bytes, secondary: cycles
    if (Bytes != Other.Bytes)
      return Bytes < Other.Bytes;
    return Cycles < Other.Cycles;
  }

  bool operator==(const Cost &Other) const {
    return Bytes == Other.Bytes && Cycles == Other.Cycles;
  }

  bool operator<=(const Cost &Other) const {
    return *this < Other || *this == Other;
  }

  Cost operator+(const Cost &Other) const {
    return Cost(Bytes + Other.Bytes, Cycles + Other.Cycles);
  }
};

/// Abstract cost model interface.
/// Targets implement this to provide instruction costs.
class CostModel {
public:
  virtual ~CostModel() = default;

  /// Get the cost of a single instruction.
  virtual Cost getInstructionCost(const MCInst &Inst) const = 0;

  /// Get the cost of an instruction sequence.
  Cost getSequenceCost(ArrayRef<MCInst> Seq) const {
    Cost Total;
    for (const auto &Inst : Seq)
      Total = Total + getInstructionCost(Inst);
    return Total;
  }
};

//===----------------------------------------------------------------------===//
// Instruction Templates (for enumeration)
//===----------------------------------------------------------------------===//

/// Describes how to enumerate operands for an instruction.
struct OperandTemplate {
  enum Kind {
    None,           // No operand
    Imm8,           // 8-bit immediate (0-255)
    Imm8Subset,     // 8-bit immediate from a subset of values
    Imm16,          // 16-bit immediate
    ZeroPage,       // Zero page address (from allowed set)
    Register,       // Register operand (from allowed set)
  };

  Kind K = None;

  /// For Imm8Subset: the specific values to try
  SmallVector<uint8_t, 16> Values;

  static OperandTemplate none() { return {None, {}}; }
  static OperandTemplate imm8() { return {Imm8, {}}; }
  static OperandTemplate imm8Subset(ArrayRef<uint8_t> Vals) {
    OperandTemplate T;
    T.K = Imm8Subset;
    T.Values.assign(Vals.begin(), Vals.end());
    return T;
  }
};

/// Template for generating instruction variants.
struct InstructionTemplate {
  unsigned Opcode;
  SmallVector<OperandTemplate, 2> Operands;

  InstructionTemplate(unsigned Op) : Opcode(Op) {}
  InstructionTemplate(unsigned Op, OperandTemplate Op0)
      : Opcode(Op), Operands({Op0}) {}
  InstructionTemplate(unsigned Op, OperandTemplate Op0, OperandTemplate Op1)
      : Opcode(Op), Operands({Op0, Op1}) {}
};

//===----------------------------------------------------------------------===//
// Enumerator Configuration
//===----------------------------------------------------------------------===//

/// Configuration for the enumerator.
struct EnumeratorConfig {
  /// Maximum sequence length.
  unsigned MaxLength = 5;

  /// Maximum cost budget.
  Cost MaxCost = {16, 32};

  /// Instruction templates to enumerate.
  SmallVector<InstructionTemplate, 64> Templates;

  /// Enable pruning heuristics.
  bool EnablePruning = true;

  /// Verbose output.
  bool Verbose = false;
};

//===----------------------------------------------------------------------===//
// Enumerator
//===----------------------------------------------------------------------===//

/// Statistics from enumeration.
struct EnumeratorStats {
  uint64_t TotalGenerated = 0;
  uint64_t Pruned = 0;
  uint64_t CostPruned = 0;
  uint64_t Emitted = 0;
};

/// Callback for receiving candidate sequences.
/// Return true to continue, false to stop.
using CandidateCallback =
    std::function<bool(ArrayRef<MCInst> Seq, const Cost &SeqCost)>;

/// Enumerates instruction sequences.
class Enumerator {
public:
  Enumerator(const MCInstrInfo &MII, const MCSubtargetInfo &STI,
             const CostModel &CM, const EnumeratorConfig &Config);

  /// Run enumeration, calling callback for each valid sequence.
  void run(CandidateCallback Callback);

  /// Get statistics.
  const EnumeratorStats &getStats() const { return Stats; }

private:
  const MCInstrInfo &MII;
  const MCSubtargetInfo &STI;
  const CostModel &CM;
  EnumeratorConfig Config;
  EnumeratorStats Stats;

  /// Current sequence being built.
  SmallVector<MCInst, 8> Current;

  /// Current cost.
  Cost CurrentCost;

  /// Callback.
  CandidateCallback Callback;

  /// Recursive enumeration.
  bool enumerate(unsigned Depth);

  /// Try adding instructions from a template.
  bool tryTemplate(const InstructionTemplate &Tmpl, unsigned Depth);

  /// Check if adding this instruction should be pruned.
  bool shouldPrune(const MCInst &Inst) const;
};

//===----------------------------------------------------------------------===//
// MOS-Specific Implementation
//===----------------------------------------------------------------------===//

namespace MOS {

/// Create a cost model for MOS targets.
std::unique_ptr<CostModel> createCostModel(const MCInstrInfo &MII,
                                           const MCSubtargetInfo &STI);

/// Get default enumerator config for register-only operations.
EnumeratorConfig getRegisterOnlyConfig(const MCInstrInfo &MII);

/// Get enumerator config for arithmetic operations.
EnumeratorConfig getArithmeticConfig(const MCInstrInfo &MII);

/// Get enumerator config for shift/rotate operations.
EnumeratorConfig getShiftConfig(const MCInstrInfo &MII);

/// Extract cycle count from TSFlags.
inline unsigned getCyclesFromTSFlags(uint64_t TSFlags) {
  return (TSFlags >> 4) & 0x1F;
}

/// Extract flag effects from TSFlags.
/// Returns 2-bit encoding: 0=unaffected, 1=modified, 2=set, 3=cleared
inline unsigned getFlagNFromTSFlags(uint64_t TSFlags) {
  return (TSFlags >> 13) & 0x3;
}
inline unsigned getFlagVFromTSFlags(uint64_t TSFlags) {
  return (TSFlags >> 15) & 0x3;
}
inline unsigned getFlagZFromTSFlags(uint64_t TSFlags) {
  return (TSFlags >> 23) & 0x3;
}
inline unsigned getFlagCFromTSFlags(uint64_t TSFlags) {
  return (TSFlags >> 25) & 0x3;
}

} // namespace MOS

} // namespace superopt
} // namespace llvm

#endif // LLVM_LIB_TARGET_MOS_SUPEROPT_SUPEROPT_H
