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
#include "llvm/Emulator/Trace.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "mos-context"

// Include the generated instruction enum
#define GET_INSTRINFO_ENUM
#include "MOSGenInstrInfo.inc"

using namespace llvm;
using namespace llvm::MOS;

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
  execute(Inst);

  // Advance PC if instruction didn't modify it
  if (!PCModified)
    PC += InstrInfo->get(Inst.getOpcode()).getSize();

  return true;
}

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
