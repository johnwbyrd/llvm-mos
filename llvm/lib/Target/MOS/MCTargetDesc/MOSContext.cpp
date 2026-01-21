//===-- MOSContext.cpp - MOS Execution Context Implementation ---*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements MOS::Context using TableGen-generated instruction
// semantics via the MOSSail/SailImpl composition pattern.
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

using namespace llvm;
using namespace llvm::MOS;

//===----------------------------------------------------------------------===//
// MOSSailImpl - External function implementations
//===----------------------------------------------------------------------===//

uint64_t MOSSailImpl::read_mem(zMem_read_requestzIbzCuzCuzK req, int64_t,
                                uint64_t, int64_t) {
  return Ctx.read(static_cast<uint16_t>(req.zpa));
}

uint64_t MOSSailImpl::read_mem_ifetch(zMem_read_requestzIbzCuzCuzK req, int64_t,
                                       uint64_t, int64_t) {
  return Ctx.read(static_cast<uint16_t>(req.zpa));
}

uint64_t MOSSailImpl::read_mem_exclusive(zMem_read_requestzIbzCuzCuzK req,
                                          int64_t, uint64_t, int64_t) {
  return Ctx.read(static_cast<uint16_t>(req.zpa));
}

bool MOSSailImpl::write_mem(zMem_write_requestzIbzCuzCuzK req, int64_t,
                             uint64_t, int64_t, uint64_t data) {
  Ctx.write(static_cast<uint16_t>(req.zpa), static_cast<uint8_t>(data));
  return true;
}

bool MOSSailImpl::write_mem_exclusive(zMem_write_requestzIbzCuzCuzK req,
                                       int64_t, uint64_t, int64_t,
                                       uint64_t data) {
  Ctx.write(static_cast<uint16_t>(req.zpa), static_cast<uint8_t>(data));
  return true;
}

//===----------------------------------------------------------------------===//
// Construction / Destruction
//===----------------------------------------------------------------------===//

Context::Context(const MCDisassembler *Disasm, const MCInstrInfo *II)
    : Disassembler(Disasm), InstrInfo(II) {
  Sail.initLets();
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
  C = Z = I = D = V = N = false;
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
  if (Sail.zcheckAndHandleNMI()) {
    // NMI was taken - add cycles for interrupt handling
    Cycles += 7;
    return true;
  }
  if (Sail.zcheckAndHandleIRQ()) {
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
        {"P", Sail.zgetP(), 8},
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

void Context::execute(const MCInst &Inst) {
  // Convert MCInst to SAIL instruction variant and execute via SAIL
  zinstruction SailInst = mcInstToSail(Inst);
  Sail.zexecute(SailInst);
}

//===----------------------------------------------------------------------===//
// Register Access (for LLDB integration)
//===----------------------------------------------------------------------===//

// DWARF register numbers from MOSRegisterInfo.td (single source of truth):
// A=0, X=1, Y=2, P=3, S=4, PC=5
// This must match the DwarfRegNum<> values in the .td file.

unsigned Context::getNumRegisters() const {
  return 6; // A, X, Y, P, S, PC
}

bool Context::readRegister(unsigned DwarfRegNum, void *Buf,
                           size_t BufSize) const {
  switch (DwarfRegNum) {
  case 0: // A
    if (BufSize < 1)
      return false;
    *static_cast<uint8_t *>(Buf) = A;
    return true;
  case 1: // X
    if (BufSize < 1)
      return false;
    *static_cast<uint8_t *>(Buf) = X;
    return true;
  case 2: // Y
    if (BufSize < 1)
      return false;
    *static_cast<uint8_t *>(Buf) = Y;
    return true;
  case 3: // P (processor status) - assembled from individual flags
    // 6502 status: N V - B D I Z C (bits 7-0)
    // Bit 5 is always 1, B (bit 4) is 0 when read (only set on push)
    if (BufSize < 1)
      return false;
    *static_cast<uint8_t *>(Buf) = (N << 7) | (V << 6) | 0x20 | (D << 3) |
                                   (I << 2) | (Z << 1) | C;
    return true;
  case 4: // S
    if (BufSize < 1)
      return false;
    *static_cast<uint8_t *>(Buf) = S;
    return true;
  case 5: // PC
    if (BufSize < 2)
      return false;
    *static_cast<uint16_t *>(Buf) = PC;
    return true;
  default:
    return false;
  }
}

bool Context::writeRegister(unsigned DwarfRegNum, const void *Buf,
                            size_t BufSize) {
  switch (DwarfRegNum) {
  case 0: // A
    if (BufSize < 1)
      return false;
    A = *static_cast<const uint8_t *>(Buf);
    return true;
  case 1: // X
    if (BufSize < 1)
      return false;
    X = *static_cast<const uint8_t *>(Buf);
    return true;
  case 2: // Y
    if (BufSize < 1)
      return false;
    Y = *static_cast<const uint8_t *>(Buf);
    return true;
  case 3: // P (processor status)
    if (BufSize < 1)
      return false;
    Sail.zsetP(*static_cast<const uint8_t *>(Buf));
    return true;
  case 4: // S
    if (BufSize < 1)
      return false;
    S = *static_cast<const uint8_t *>(Buf);
    return true;
  case 5: // PC
    if (BufSize < 2)
      return false;
    PC = *static_cast<const uint16_t *>(Buf);
    return true;
  default:
    return false;
  }
}

bool Context::writeRegisterNoLog(unsigned DwarfRegNum, const void *Buf,
                                 size_t BufSize) {
  // For MOS, writeRegister doesn't log (the SAIL-generated code does its own
  // thing). This is used during checkpoint restore.
  return writeRegister(DwarfRegNum, Buf, BufSize);
}
