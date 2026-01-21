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
// Architecture:
//   MOSSail      - Generated base class containing all SAIL code (registers,
//                  methods, pure virtual externals)
//   SailImpl     - Inner class implementing the externals via Context's
//                  read()/write() methods
//   Context      - Owns SailImpl, provides clean register aliases and
//                  emu::Context interface
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_MOS_MOSCONTEXT_H
#define LLVM_LIB_TARGET_MOS_MOSCONTEXT_H

#include "MOSMCTargetDesc.h"
#include "llvm/Emulator/Context.h"
#include <cstdint>
#include <variant>

namespace llvm {

class MCDisassembler;
class MCInst;
class MCInstrInfo;

namespace MOS {

//===----------------------------------------------------------------------===//
// SAIL-generated types and mapping function at namespace scope
//===----------------------------------------------------------------------===//

#define GET_SAIL_CLASS_TYPES
#include "MOSGenEmulator.inc"
#undef GET_SAIL_CLASS_TYPES

//===----------------------------------------------------------------------===//
// MOSSail - Generated base class containing all SAIL code
//===----------------------------------------------------------------------===//

/// Base class containing all SAIL-generated code.
/// Derived class must implement the pure virtual external functions.
class MOSSail {
  friend class Context;  // Allow Context to access protected members

public:
  virtual ~MOSSail() = default;

#define GET_SAIL_CLASS_BODY
#include "MOSGenEmulator.inc"
#undef GET_SAIL_CLASS_BODY
};

//===----------------------------------------------------------------------===//
// Context - MOS 6502-family execution context
//===----------------------------------------------------------------------===//

class Context;  // Forward declaration

/// Implements SAIL external functions by delegating to Context's memory.
class MOSSailImpl : public MOSSail {
  Context &Ctx;

public:
  MOSSailImpl(Context &C) : Ctx(C) {}

  // Memory read - all variants use the same simple implementation
  uint64_t read_mem(zMem_read_requestzIbzCuzCuzK req, int64_t, uint64_t,
                    int64_t) override;
  uint64_t read_mem_ifetch(zMem_read_requestzIbzCuzCuzK req, int64_t,
                           uint64_t, int64_t) override;
  uint64_t read_mem_exclusive(zMem_read_requestzIbzCuzCuzK req, int64_t,
                              uint64_t, int64_t) override;

  // Memory write - all variants use the same simple implementation
  bool write_mem(zMem_write_requestzIbzCuzCuzK req, int64_t, uint64_t,
                 int64_t, uint64_t data) override;
  bool write_mem_exclusive(zMem_write_requestzIbzCuzCuzK req, int64_t,
                           uint64_t, int64_t, uint64_t data) override;

  // Capability tags - not used on 6502
  bool emulator_read_tag(int64_t, uint64_t) override { return false; }
  void emulator_write_tag(int64_t, uint64_t, bool) override {}
};

/// MOS 6502-family execution context.
class Context : public emu::Context {
  MOSSailImpl Sail{*this};

public:
  //===--------------------------------------------------------------------===//
  // Clean register aliases (reference SAIL storage)
  //===--------------------------------------------------------------------===//

  uint8_t &A = Sail.zA;
  uint8_t &X = Sail.zX;
  uint8_t &Y = Sail.zY;
  uint8_t &S = Sail.zS;
  uint16_t &PC = Sail.zPC;
  uint16_t &NextPC = Sail.zNextPC;
  uint8_t &N = Sail.zN;
  uint8_t &V = Sail.zV;
  uint8_t &D = Sail.zD;
  uint8_t &I = Sail.zI;
  uint8_t &Z = Sail.zZ;
  uint8_t &C = Sail.zC;
  uint8_t &IRQPending = Sail.zIRQPending;
  uint8_t &NMIPending = Sail.zNMIPending;
  int64_t &Cycles = Sail.zCycles;

  //===--------------------------------------------------------------------===//
  // Additional CPU State (not in SAIL)
  //===--------------------------------------------------------------------===//

  bool Halted = false;
  int ExitCode = 0;
  bool DidPageCross = false;

  static constexpr uint16_t IrqVector = 0xFFFE;
  static constexpr uint16_t NmiVector = 0xFFFA;

  //===--------------------------------------------------------------------===//
  // Construction
  //===--------------------------------------------------------------------===//

  Context(const MCDisassembler *Disasm, const MCInstrInfo *II);
  ~Context();

  //===--------------------------------------------------------------------===//
  // emu::Context Interface
  //===--------------------------------------------------------------------===//

  bool step() override;
  void reset() override;

  uint64_t getPC() const override { return Sail.zPC; }
  void setPC(uint64_t NewPC) override { Sail.zPC = static_cast<uint16_t>(NewPC); }
  uint64_t getCycles() const override { return Sail.zCycles; }
  bool isHalted() const override { return Halted; }
  void halt(int Code = 0) override;
  int getExitCode() const override { return ExitCode; }
  unsigned getAddressBits() const override { return 16; }

  //===--------------------------------------------------------------------===//
  // Register Access (for LLDB integration)
  //===--------------------------------------------------------------------===//

  unsigned getNumRegisters() const override;
  bool readRegister(unsigned DwarfRegNum, void *Buf,
                    size_t BufSize) const override;
  bool writeRegister(unsigned DwarfRegNum, const void *Buf,
                     size_t BufSize) override;
  bool writeRegisterNoLog(unsigned DwarfRegNum, const void *Buf,
                          size_t BufSize) override;

  void assertIRQ() override { Sail.zIRQPending = 1; }
  void deassertIRQ() override { Sail.zIRQPending = 0; }
  void assertNMI() override { Sail.zNMIPending = 1; }

  //===--------------------------------------------------------------------===//
  // Helper functions
  //===--------------------------------------------------------------------===//

  bool pageCrossed(uint16_t Addr1, uint16_t Addr2) {
    return (Addr1 & 0xFF00) != (Addr2 & 0xFF00);
  }

  /// Get access to SAIL implementation (for internal use)
  MOSSailImpl &getSail() { return Sail; }

private:
  const MCDisassembler *Disassembler;
  const MCInstrInfo *InstrInfo;

  void execute(const MCInst &Inst);
};

} // namespace MOS
} // namespace llvm

#endif // LLVM_LIB_TARGET_MOS_MOSCONTEXT_H
