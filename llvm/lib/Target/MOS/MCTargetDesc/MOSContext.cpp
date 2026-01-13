//===-- MOSContext.cpp - MOS Execution Context Implementation ---*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements MOS::Context using TableGen-generated instruction
// semantics.
//
//===----------------------------------------------------------------------===//

#include "MOSContext.h"
#include "MOSMCTargetDesc.h"
#include "llvm/Emulator/Trace.h"
#include "llvm/MC/MCDisassembler/MCDisassembler.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "mos-context"

// Note: MOSMCTargetDesc.h already includes MOSGenInstrInfo.inc with GET_INSTRINFO_ENUM

using namespace llvm;
using namespace llvm::MOS;

//===----------------------------------------------------------------------===//
// Construction / Destruction
//===----------------------------------------------------------------------===//

Context::Context(const MCDisassembler *Disasm, const MCInstrInfo *II)
    : Disassembler(Disasm), InstrInfo(II) {}

Context::~Context() {
  delete Disassembler;
  delete InstrInfo;
}

//===----------------------------------------------------------------------===//
// Context Interface Implementation
//===----------------------------------------------------------------------===//

void Context::reset() {
  A = 0;
  X = 0;
  Y = 0;
  S = 0xFF;
  C = Z = I = D = B = V = N = false;
  Cycles = 0;
  Halted = false;
  ExitCode_ = 0;

  // Load reset vector
  PC = read16(0xFFFC);
}

void Context::halt(int ExitCode) {
  Halted = true;
  ExitCode_ = ExitCode;
}

bool Context::step() {
  if (Halted)
    return true;

  // Fetch and decode instruction
  MCInst Inst;
  uint64_t Size;

  // Read bytes from memory for disassembly
  uint8_t InstBytes[16];
  for (int I = 0; I < 16; ++I)
    InstBytes[I] = read(PC + I);

  ArrayRef<uint8_t> InstBytesRef(InstBytes, 16);

  auto Status = Disassembler->getInstruction(Inst, Size, InstBytesRef, PC,
                                              nulls());

  if (Status != MCDisassembler::Success) {
    errs() << "Failed to decode instruction at PC=$" << Twine::utohexstr(PC)
           << " bytes: " << format("%02X %02X %02X", InstBytes[0], InstBytes[1], InstBytes[2])
           << "\n";
    Halted = true;
    ExitCode_ = 1;
    return false;
  }

  // Trace if enabled
  if (Tracing && Trace) {
    // Build register state for trace
    emu::TraceReg Regs[] = {
        {"A", A, 8},
        {"X", X, 8},
        {"Y", Y, 8},
        {"S", S, 8},
        {"P", getP(), 8},
    };
    Trace->traceInstruction(Cycles, PC, Inst, Regs);
  } else if (Tracing) {
    // Fallback to simple output if no TraceWriter
    errs() << format("$%04X: ", PC);
    if (InstPrinter && STI) {
      InstPrinter->printInst(&Inst, 0, "", *STI, errs());
    } else {
      errs() << "opcode=" << Inst.getOpcode();
    }
    errs() << "\n";
  }

  // Execute the instruction
  PCModified = false;
  DidPageCross = false;
  uint16_t PrePC = PC;
  execute(Inst);

  // Accumulate cycles from instruction TSFlags
  const MCInstrDesc &Desc = InstrInfo->get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;
  unsigned BaseCycles = MOS::getCycles(TSFlags);
  unsigned PageCrossPenalty = DidPageCross ? MOS::getPageCrossCycles(TSFlags) : 0;
  Cycles += BaseCycles + PageCrossPenalty;

  // Handle branch page-cross penalty
  // Branch Emulate code already adds +1 for taken (via Cycles++).
  // We add another +1 if the branch crossed a page boundary.
  if (Desc.isBranch() && PCModified) {
    uint16_t NextPC = PrePC + Desc.getSize();
    if (pageCrossed(NextPC, PC)) {
      Cycles += 1;
    }
  }

  // Advance PC if instruction didn't modify it
  if (!PCModified)
    PC += Desc.getSize();

  return true;
}

//===----------------------------------------------------------------------===//
// Helper Methods
//===----------------------------------------------------------------------===//

void Context::setNZ(uint8_t Val) {
  N = (Val >> 7) & 1;
  Z = Val == 0;
}

void Context::push(uint8_t Val) { write(0x100 + S--, Val); }

uint8_t Context::pull() { return read(0x100 + ++S); }

void Context::push16(uint16_t Val) {
  push(Val >> 8);
  push(Val & 0xFF);
}

uint16_t Context::pull16() {
  uint8_t Lo = pull();
  return Lo | (pull() << 8);
}

bool Context::pageCrossed(uint16_t Addr1, uint16_t Addr2) {
  return (Addr1 & 0xFF00) != (Addr2 & 0xFF00);
}

uint8_t Context::getP() const {
  return (N << 7) | (V << 6) | (1 << 5) | (B << 4) | (D << 3) | (I << 2) |
         (Z << 1) | C;
}

void Context::setP(uint8_t P) {
  N = (P >> 7) & 1;
  V = (P >> 6) & 1;
  B = (P >> 4) & 1;
  D = (P >> 3) & 1;
  I = (P >> 2) & 1;
  Z = (P >> 1) & 1;
  C = P & 1;
}

void Context::doADC(uint8_t Val) {
  if (D) {
    // Decimal mode
    uint8_t Lo = (A & 0x0F) + (Val & 0x0F) + C;
    uint8_t Hi = (A >> 4) + (Val >> 4);
    if (Lo > 9) {
      Lo -= 10;
      Hi++;
    }
    Z = ((A + Val + C) & 0xFF) == 0;
    N = Hi & 0x08;
    V = ~(A ^ Val) & (A ^ (Hi << 4)) & 0x80;
    if (Hi > 9) {
      Hi -= 10;
      C = true;
    } else {
      C = false;
    }
    A = (Hi << 4) | (Lo & 0x0F);
  } else {
    // Binary mode
    uint16_t Sum = A + Val + C;
    C = Sum > 0xFF;
    V = ~(A ^ Val) & (A ^ Sum) & 0x80;
    A = Sum & 0xFF;
    setNZ(A);
  }
}

void Context::doSBC(uint8_t Val) {
  if (D) {
    // Decimal mode
    int Lo = (A & 0x0F) - (Val & 0x0F) - !C;
    int Hi = (A >> 4) - (Val >> 4);
    if (Lo < 0) {
      Lo += 10;
      Hi--;
    }
    if (Hi < 0) {
      Hi += 10;
      C = false;
    } else {
      C = true;
    }
    A = (Hi << 4) | (Lo & 0x0F);
    // N, Z, V set based on binary result
    uint16_t Diff = A - Val - !C;
    Z = (Diff & 0xFF) == 0;
    N = Diff & 0x80;
    V = (A ^ Val) & (A ^ Diff) & 0x80;
  } else {
    // Binary mode
    uint16_t Diff = A - Val - !C;
    C = Diff <= 0xFF; // No borrow
    V = (A ^ Val) & (A ^ Diff) & 0x80;
    A = Diff & 0xFF;
    setNZ(A);
  }
}

//===----------------------------------------------------------------------===//
// Instruction Execution
//===----------------------------------------------------------------------===//

void Context::execute(const MCInst &Inst) {
  unsigned Opcode = Inst.getOpcode();

  switch (Opcode) {
#define GET_EMULATOR_CASES
#include "MOSGenEmulator.inc"
#undef GET_EMULATOR_CASES

  default:
    // Unknown instruction - halt
    LLVM_DEBUG(dbgs() << "Unknown opcode: " << Opcode << "\n");
    Halted = true;
    ExitCode_ = 1;
    break;
  }
}
