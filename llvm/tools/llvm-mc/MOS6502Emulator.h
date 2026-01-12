//===-- MOS6502Emulator.h - 6502 Emulator for llvm-mc -----------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a simple 6502 emulator for use with llvm-mc --run.
// The emulator executes decoded MCInst instructions directly.
//
// This is a prototype implementation. Eventually, the emulateInstruction()
// method will be auto-generated from TableGen definitions in MOSEmulator.td.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TOOLS_LLVM_MC_MOS6502EMULATOR_H
#define LLVM_TOOLS_LLVM_MC_MOS6502EMULATOR_H

#include "llvm/MC/MCInst.h"
#include <cstdint>
#include <functional>

namespace llvm {

/// A simple 6502 CPU emulator that executes MCInst instructions.
class MOS6502Emulator {
public:
  //===--------------------------------------------------------------------===//
  // CPU State
  //===--------------------------------------------------------------------===//

  // Registers
  uint8_t A = 0;   // Accumulator
  uint8_t X = 0;   // X index register
  uint8_t Y = 0;   // Y index register
  uint8_t S = 0xFF; // Stack pointer (in page 1: $0100-$01FF)
  uint16_t PC = 0; // Program counter

  // Status flags
  bool C = false; // Carry
  bool Z = false; // Zero
  bool I = false; // Interrupt disable
  bool D = false; // Decimal mode (not emulated)
  bool B = false; // Break (only exists on stack)
  bool V = false; // Overflow
  bool N = false; // Negative

  // Execution state
  uint64_t Cycles = 0;
  bool Halted = false;

  //===--------------------------------------------------------------------===//
  // Memory Interface
  //===--------------------------------------------------------------------===//

  std::function<uint8_t(uint16_t)> read;
  std::function<void(uint16_t, uint8_t)> write;

  //===--------------------------------------------------------------------===//
  // Helper Methods
  //===--------------------------------------------------------------------===//

  void setNZ(uint8_t val) {
    N = (val >> 7) & 1;
    Z = val == 0;
  }

  void push(uint8_t val) { write(0x100 + S--, val); }
  uint8_t pull() { return read(0x100 + ++S); }

  void push16(uint16_t val) {
    push(val >> 8);
    push(val & 0xFF);
  }

  uint16_t pull16() {
    uint8_t lo = pull();
    return lo | (pull() << 8);
  }

  bool pageCrossed(uint16_t a, uint16_t b) {
    return (a & 0xFF00) != (b & 0xFF00);
  }

  // Get processor status as a byte
  uint8_t getP() const {
    return (N << 7) | (V << 6) | (1 << 5) | (B << 4) |
           (D << 3) | (I << 2) | (Z << 1) | C;
  }

  // Set processor status from a byte
  void setP(uint8_t p) {
    N = (p >> 7) & 1;
    V = (p >> 6) & 1;
    // Bit 5 is always 1
    B = (p >> 4) & 1;
    D = (p >> 3) & 1;
    I = (p >> 2) & 1;
    Z = (p >> 1) & 1;
    C = p & 1;
  }

  //===--------------------------------------------------------------------===//
  // Instruction Execution
  //===--------------------------------------------------------------------===//

  /// Execute a single instruction.
  /// Returns the number of bytes consumed (for PC advancement).
  /// If the instruction modifies PC directly (branches, jumps), returns 0.
  unsigned emulateInstruction(const MCInst &Inst);

  /// Get the size of an instruction from its opcode.
  unsigned getInstSize(unsigned Opcode) const;
};

} // namespace llvm

#endif // LLVM_TOOLS_LLVM_MC_MOS6502EMULATOR_H
