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
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include <cstdint>

namespace llvm {
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
  bool PCModified = false; // Set by instructions that modify PC (JMP, JSR, etc.)

  //===--------------------------------------------------------------------===//
  // Construction
  //===--------------------------------------------------------------------===//

  /// Create a context with the given disassembler and instruction info.
  /// Takes ownership of both pointers.
  Context(const MCDisassembler *Disasm, const MCInstrInfo *II)
      : Disassembler(Disasm), InstrInfo(II) {}

  ~Context() {
    delete Disassembler;
    delete InstrInfo;
  }

  //===--------------------------------------------------------------------===//
  // Context Interface Implementation
  //===--------------------------------------------------------------------===//

  bool step() override;
  void reset() override;

  uint64_t getPC() const override { return PC; }
  void setPC(uint64_t NewPC) override { PC = static_cast<uint16_t>(NewPC); }
  uint64_t getCycles() const override { return Cycles; }
  bool isHalted() const override { return Halted; }
  void halt(int ExitCode = 0) override {
    Halted = true;
    ExitCode_ = ExitCode;
  }
  int getExitCode() const override { return ExitCode_; }

  /// MOS has a 16-bit address bus (64KB address space).
  unsigned getAddressBits() const override { return 16; }

  //===--------------------------------------------------------------------===//
  // Helper Methods (used by generated code)
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

  /// Get processor status as a byte.
  uint8_t getP() const {
    return (N << 7) | (V << 6) | (1 << 5) | (B << 4) | (D << 3) | (I << 2) |
           (Z << 1) | C;
  }

  /// Set processor status from a byte.
  void setP(uint8_t p) {
    N = (p >> 7) & 1;
    V = (p >> 6) & 1;
    B = (p >> 4) & 1;
    D = (p >> 3) & 1;
    I = (p >> 2) & 1;
    Z = (p >> 1) & 1;
    C = p & 1;
  }

  /// ADC with decimal mode support.
  void doADC(uint8_t val) {
    if (D) {
      // Decimal mode
      uint8_t lo = (A & 0x0F) + (val & 0x0F) + C;
      uint8_t hi = (A >> 4) + (val >> 4);
      if (lo > 9) {
        lo -= 10;
        hi++;
      }
      Z = ((A + val + C) & 0xFF) == 0;
      N = hi & 0x08;
      V = ~(A ^ val) & (A ^ (hi << 4)) & 0x80;
      if (hi > 9) {
        hi -= 10;
        C = true;
      } else {
        C = false;
      }
      A = (hi << 4) | (lo & 0x0F);
    } else {
      // Binary mode
      uint16_t sum = A + val + C;
      C = sum > 0xFF;
      V = ~(A ^ val) & (A ^ sum) & 0x80;
      A = sum & 0xFF;
      setNZ(A);
    }
  }

  /// SBC with decimal mode support.
  void doSBC(uint8_t val) {
    if (D) {
      // Decimal mode
      int lo = (A & 0x0F) - (val & 0x0F) - !C;
      int hi = (A >> 4) - (val >> 4);
      if (lo < 0) {
        lo += 10;
        hi--;
      }
      if (hi < 0) {
        hi += 10;
        C = false;
      } else {
        C = true;
      }
      A = (hi << 4) | (lo & 0x0F);
      // N, Z, V set based on binary result
      uint16_t diff = A - val - !C;
      Z = (diff & 0xFF) == 0;
      N = diff & 0x80;
      V = (A ^ val) & (A ^ diff) & 0x80;
    } else {
      // Binary mode
      uint16_t diff = A - val - !C;
      C = diff <= 0xFF; // No borrow
      V = (A ^ val) & (A ^ diff) & 0x80;
      A = diff & 0xFF;
      setNZ(A);
    }
  }

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
