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
#include <variant>

namespace llvm {

class MCDisassembler;
class MCInst;
class MCInstrInfo;

namespace MOS {

//===----------------------------------------------------------------------===//
// SAIL-generated types (enums, unions) at namespace scope
//===----------------------------------------------------------------------===//

#define GET_EMULATOR_TYPES
#include "MOSGenEmulator.inc"
#undef GET_EMULATOR_TYPES

/// MOS 6502-family execution context.
/// Uses TableGen-generated instruction semantics from MOSGenEmulator.inc.
class Context : public emu::Context {
public:
  //===--------------------------------------------------------------------===//
  // SAIL-generated registers (z-prefixed, canonical storage)
  //===--------------------------------------------------------------------===//

#define GET_EMULATOR_MEMBERS
#include "MOSGenEmulator.inc"
#undef GET_EMULATOR_MEMBERS

  //===--------------------------------------------------------------------===//
  // Additional CPU State (not in SAIL)
  //===--------------------------------------------------------------------===//

  bool Halted = false;
  int ExitCode = 0;
  bool DidPageCross = false; // Set by indexed addressing modes when crossing page

  // Interrupt vectors
  static constexpr uint16_t IrqVector = 0xFFFE;
  static constexpr uint16_t NmiVector = 0xFFFA;

  //===--------------------------------------------------------------------===//
  // Non-z-prefixed aliases for external use
  //===--------------------------------------------------------------------===//

  uint8_t &A = zA;
  uint8_t &X = zX;
  uint8_t &Y = zY;
  uint8_t &S = zS;
  uint16_t &PC = zPC;
  uint16_t &NextPC = zNextPC;
  uint8_t &N = zN;
  uint8_t &V = zV;
  uint8_t &D = zD;
  uint8_t &I = zI;
  uint8_t &Z = zZ;
  uint8_t &C = zC;
  uint8_t &IRQPending = zIRQPending;
  uint8_t &NMIPending = zNMIPending;
  int64_t &Cycles = zCycles;

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
  int getExitCode() const override { return ExitCode; }

  /// MOS has a 16-bit address bus (64KB address space).
  unsigned getAddressBits() const override { return 16; }

  //===--------------------------------------------------------------------===//
  // Register Access (for LLDB integration)
  // Uses DWARF register numbers from MOSRegisterInfo.td
  //===--------------------------------------------------------------------===//

  unsigned getNumRegisters() const override;
  bool readRegister(unsigned DwarfRegNum, void *Buf,
                    size_t BufSize) const override;
  bool writeRegister(unsigned DwarfRegNum, const void *Buf,
                     size_t BufSize) override;
  bool writeRegisterNoLog(unsigned DwarfRegNum, const void *Buf,
                          size_t BufSize) override;

  /// Assert IRQ line (level-triggered).
  void assertIRQ() override { IRQPending = 1; }

  /// Deassert IRQ line.
  void deassertIRQ() override { IRQPending = 0; }

  /// Assert NMI (edge-triggered).
  void assertNMI() override { NMIPending = 1; }

  //===--------------------------------------------------------------------===//
  // External functions (called by SAIL-generated code but defined here)
  // These are the interface between SAIL code and C++ runtime.
  //===--------------------------------------------------------------------===//

  /// Memory access - matches SAIL readMem/writeMem names.
  uint8_t readMem(uint16_t Addr) { return read(Addr); }
  void writeMem(uint16_t Addr, uint8_t Val) { write(Addr, Val); }

  /// Check if two addresses are in different pages (not in SAIL).
  bool pageCrossed(uint16_t Addr1, uint16_t Addr2) {
    return (Addr1 & 0xFF00) != (Addr2 & 0xFF00);
  }

  //===--------------------------------------------------------------------===//
  // SAIL memory model primitives (simplified for basic emulator)
  // These implement the complex SAIL memory interface using our simple read/write.
  //===--------------------------------------------------------------------===//

  /// Low-level memory read - extracts address from request, calls read()
  uint64_t zread_memz3zIRMem_read_requestzIbzCuzCuzKzK(
      zMem_read_requestzIbzCuzCuzK req, int64_t, uint64_t, int64_t) {
    return read(static_cast<uint16_t>(req.zpa));
  }

  uint64_t zread_mem_ifetchz3zIRMem_read_requestzIbzCuzCuzKzK(
      zMem_read_requestzIbzCuzCuzK req, int64_t, uint64_t, int64_t) {
    return read(static_cast<uint16_t>(req.zpa));
  }

  uint64_t zread_mem_exclusivez3zIRMem_read_requestzIbzCuzCuzKzK(
      zMem_read_requestzIbzCuzCuzK req, int64_t, uint64_t, int64_t) {
    return read(static_cast<uint16_t>(req.zpa));
  }

  /// Low-level memory write - extracts address/data from request, calls write()
  bool zwrite_memz3zIRMem_write_requestzIbzCuzCuzKzK(
      zMem_write_requestzIbzCuzCuzK req, int64_t, uint64_t, int64_t, uint64_t data) {
    write(static_cast<uint16_t>(req.zpa), static_cast<uint8_t>(data));
    return true;
  }

  bool zwrite_mem_exclusivez3zIRMem_write_requestzIbzCuzCuzKzK(
      zMem_write_requestzIbzCuzCuzK req, int64_t, uint64_t, int64_t, uint64_t data) {
    write(static_cast<uint16_t>(req.zpa), static_cast<uint8_t>(data));
    return true;
  }

  /// Capability tags (not used in basic 6502 - always return false/no-op)
  bool zread_tagz3(int64_t, uint64_t) { return false; }
  void zwrite_tagz3(int64_t, uint64_t, bool) {}

  //===--------------------------------------------------------------------===//
  // SAIL-generated helper functions (as class methods)
  //===--------------------------------------------------------------------===//

#define GET_EMULATOR_METHODS
#include "MOSGenEmulator.inc"
#undef GET_EMULATOR_METHODS

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
