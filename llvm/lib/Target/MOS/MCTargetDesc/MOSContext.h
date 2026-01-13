//===-- MOSContext.h - MOS Execution Context --------------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines MOS::Context, the MOS-specific execution context that
// uses TableGen-generated instruction semantics from MOSGenEmulator.inc.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MOS_MOSCONTEXT_H
#define LLVM_LIB_TARGET_MOS_MOSCONTEXT_H

#include "llvm/Emulator/Context.h"
#include <cstdint>

namespace llvm {

class MCDisassembler;
class MCInst;
class MCInstrInfo;

namespace MOS {

/// MOS 6502-family execution context.
/// Uses TableGen-generated instruction semantics from MOSGenEmulator.inc.
class Context : public emu::Context {
public:
  //===--------------------------------------------------------------------===//
  // CPU State
  //===--------------------------------------------------------------------===//

  // Registers
  uint8_t A = 0;    // Accumulator
  uint8_t X = 0;    // X index register
  uint8_t Y = 0;    // Y index register
  uint8_t S = 0xFF; // Stack pointer (in page 1: $0100-$01FF)
  uint16_t PC = 0;  // Program counter

  // Status flags
  bool C = false; // Carry
  bool Z = false; // Zero
  bool I = false; // Interrupt disable
  bool D = false; // Decimal mode
  bool B = false; // Break (only exists on stack)
  bool V = false; // Overflow
  bool N = false; // Negative

  // Execution state
  uint64_t Cycles = 0;
  bool Halted = false;
  int ExitCode_ = 0;
  bool PCModified = false;   // Set by instructions that modify PC (JMP, JSR, etc.)
  bool DidPageCross = false; // Set by indexed addressing modes when crossing page

  //===--------------------------------------------------------------------===//
  // Construction
  //===--------------------------------------------------------------------===//

  /// Create a context with the given disassembler and instruction info.
  /// Takes ownership of both pointers.
  Context(const MCDisassembler *Disasm, const MCInstrInfo *II);
  ~Context();

  //===--------------------------------------------------------------------===//
  // Context Interface Implementation
  //===--------------------------------------------------------------------===//

  bool step() override;
  void reset() override;

  uint64_t getPC() const override { return PC; }
  void setPC(uint64_t NewPC) override { PC = static_cast<uint16_t>(NewPC); }
  uint64_t getCycles() const override { return Cycles; }
  bool isHalted() const override { return Halted; }
  void halt(int ExitCode = 0) override;
  int getExitCode() const override { return ExitCode_; }

  /// MOS has a 16-bit address bus (64KB address space).
  unsigned getAddressBits() const override { return 16; }

  //===--------------------------------------------------------------------===//
  // Helper Methods (used by generated code)
  //===--------------------------------------------------------------------===//

  void setNZ(uint8_t Val);
  void push(uint8_t Val);
  uint8_t pull();
  void push16(uint16_t Val);
  uint16_t pull16();
  bool pageCrossed(uint16_t Addr1, uint16_t Addr2);

  /// Get processor status as a byte.
  uint8_t getP() const;

  /// Set processor status from a byte.
  void setP(uint8_t P);

  /// ADC with decimal mode support.
  void doADC(uint8_t Val);

  /// SBC with decimal mode support.
  void doSBC(uint8_t Val);

private:
  const MCDisassembler *Disassembler;
  const MCInstrInfo *InstrInfo;

  /// Execute a single decoded instruction.
  /// Sets PCModified if the instruction changes PC (JMP, JSR, branches, etc.)
  void execute(const MCInst &Inst);
};

} // namespace MOS
} // namespace llvm

#endif // LLVM_LIB_TARGET_MOS_MOSCONTEXT_H
