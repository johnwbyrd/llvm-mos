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
  uint16_t NextPC = 0;       // Next PC (set before execute, branches/jumps override)
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
  // Helper Methods (used by SAIL-generated code)
  // Names match SAIL specification for direct code generation.
  //===--------------------------------------------------------------------===//

  /// Memory access - matches SAIL readMem/writeMem names.
  uint8_t readMem(uint16_t Addr) { return read(Addr); }
  void writeMem(uint16_t Addr, uint8_t Val) { write(Addr, Val); }
  uint16_t readMem16(uint16_t Addr) { return read16(Addr); }

  /// Set N and Z flags based on value.
  void setNZ(uint8_t Val);

  /// Stack operations.
  void push(uint8_t Val);
  uint8_t pull();
  void push16(uint16_t Val);
  uint16_t pull16();

  /// Check if two addresses are in different pages.
  bool pageCrossed(uint16_t Addr1, uint16_t Addr2);

  /// Set the next PC (called by branches/jumps to override default).
  void setNextPC(uint16_t PC) { NextPC = PC; }

  /// Branch helper - if condition is true, set next PC to NextPC + sign-extended offset.
  void doBranch(bool Cond, uint8_t Offset) {
    if (Cond) {
      setNextPC(NextPC + (int8_t)Offset);
      Cycles++; // Taken branch adds 1 cycle
    }
  }

  /// Get processor status as a byte.
  uint8_t getP() const;

  /// Set processor status from a byte.
  void setP(uint8_t P);

  /// ADC with decimal mode support.
  void doADC(uint8_t Val);

  /// SBC with decimal mode support.
  void doSBC(uint8_t Val);

  /// Compare helper - sets N, Z, C flags based on Reg - Val.
  void doCMP(uint8_t Reg, uint8_t Val) {
    uint8_t Result = Reg - Val;
    C = Reg >= Val;
    setNZ(Result);
  }

  /// BIT test helper - sets N, V, Z flags.
  void doBIT(uint8_t Val) {
    N = (Val >> 7) & 1;
    V = (Val >> 6) & 1;
    Z = (A & Val) == 0;
  }

  /// Memory increment helper.
  void doIncMem(uint16_t EA) {
    uint8_t Val = readMem(EA) + 1;
    writeMem(EA, Val);
    setNZ(Val);
  }

  /// Memory decrement helper.
  void doDecMem(uint16_t EA) {
    uint8_t Val = readMem(EA) - 1;
    writeMem(EA, Val);
    setNZ(Val);
  }

  /// ASL memory helper.
  void doASLMem(uint16_t EA) {
    uint8_t Val = readMem(EA);
    C = (Val >> 7) & 1;
    Val <<= 1;
    writeMem(EA, Val);
    setNZ(Val);
  }

  /// LSR memory helper.
  void doLSRMem(uint16_t EA) {
    uint8_t Val = readMem(EA);
    C = Val & 1;
    Val >>= 1;
    writeMem(EA, Val);
    setNZ(Val);
  }

  /// ROL memory helper.
  void doROLMem(uint16_t EA) {
    uint8_t Val = readMem(EA);
    bool OldC = C;
    C = (Val >> 7) & 1;
    Val = (Val << 1) | OldC;
    writeMem(EA, Val);
    setNZ(Val);
  }

  /// ROR memory helper.
  void doRORMem(uint16_t EA) {
    uint8_t Val = readMem(EA);
    bool OldC = C;
    C = Val & 1;
    Val = (Val >> 1) | (OldC << 7);
    writeMem(EA, Val);
    setNZ(Val);
  }

private:
  const MCDisassembler *Disassembler;
  const MCInstrInfo *InstrInfo;

  /// Execute a single decoded instruction.
  /// Branches/jumps call set_next_pc() to override the default next PC.
  void execute(const MCInst &Inst);
};

} // namespace MOS
} // namespace llvm

#endif // LLVM_LIB_TARGET_MOS_MOSCONTEXT_H
