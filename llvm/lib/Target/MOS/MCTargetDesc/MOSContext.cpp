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
//===--------------------------------------------Conti--------------------------===//

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
    : Disassembler(Disasm), InstrInfo(II) {
  initLets();
}

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
  ExitCode = 0;

  // Reset interrupt state
  IRQPending = 0;
  NMIPending = 0;

  // Load reset vector
  PC = read16(0xFFFC);
}

void Context::halt(int Code) {
  Halted = true;
  ExitCode = Code;
}

bool Context::step() {
  if (Halted)
    return true;

  // Check for pending interrupts before executing the next instruction
  // NMI has priority over IRQ (checked first)
  if (zcheckAndHandleNMI()) {
    // NMI was taken - add cycles for interrupt handling
    Cycles += 7;
    return true;
  }
  if (zcheckAndHandleIRQ()) {
    // IRQ was taken - add cycles for interrupt handling
    Cycles += 7;
    return true;
  }

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
    ExitCode = 1;
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
        {"P", zgetP(), 8},
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

  // Get instruction descriptor and TSFlags
  const MCInstrDesc &Desc = InstrInfo->get(Inst.getOpcode());
  uint64_t TSFlags = Desc.TSFlags;

  // Set NextPC to default (instruction after this one)
  // Branches/jumps will override this via set_next_pc()
  uint16_t DefaultNextPC = PC + Desc.getSize();
  NextPC = DefaultNextPC;
  DidPageCross = false;

  // Execute the instruction
  execute(Inst);

  // Check for halt instruction
  if (MOS::getHaltEmulation(TSFlags)) {
    halt(0);
    return true;
  }

  // Accumulate cycles from instruction TSFlags
  unsigned BaseCycles = MOS::getCycles(TSFlags);
  unsigned PageCrossPenalty = DidPageCross ? MOS::getPageCrossCycles(TSFlags) : 0;
  Cycles += BaseCycles + PageCrossPenalty;

  // Handle branch page-cross penalty
  // Branch instructions add +1 for taken (via Cycles++ in generated code).
  // We add another +1 if the branch crossed a page boundary.
  if (Desc.isBranch() && NextPC != DefaultNextPC) {
    // Branch was taken (NextPC was modified)
    if (pageCrossed(DefaultNextPC, NextPC)) {
      Cycles += 1;
    }
  }

  // Commit NextPC to PC (tick_pc)
  PC = NextPC;

  return true;
}


//===----------------------------------------------------------------------===//
// Instruction Execution
//===----------------------------------------------------------------------===//

// Pull in the MCInst-to-SAIL mapping function (must be in MOS namespace)
namespace llvm {
namespace MOS {
#define GET_EMULATOR_MAPPING
#include "MOSGenEmulator.inc"
#undef GET_EMULATOR_MAPPING
} // namespace MOS
} // namespace llvm

void Context::execute(const MCInst &Inst) {
  // Convert MCInst to SAIL instruction variant and execute via SAIL
  zinstruction SailInst = mcInstToSail(Inst);
  zexecute(SailInst);
}

void Context::executeInst(const MCInst &Inst) {
  // Direct execution without fetch-decode cycle (for superoptimizer)
  execute(Inst);
}
