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

  // Interrupt state (directly set by external hardware/devices)
  uint8_t IRQPending = 0; // Hardware IRQ line asserted (level-triggered)
  uint8_t NMIPending = 0; // Non-maskable interrupt pending (edge-triggered)

  // Execution state
  uint64_t Cycles = 0;
  bool Halted = false;
  int ExitCode = 0;
  uint16_t NextPC = 0;       // Next PC (set before execute, branches/jumps override)
  bool DidPageCross = false; // Set by indexed addressing modes when crossing page

  // Interrupt vectors
  static constexpr uint16_t IrqVector = 0xFFFE;
  static constexpr uint16_t NmiVector = 0xFFFA;

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
  // SAIL register aliases (z-prefixed names for generated code)
  //===--------------------------------------------------------------------===//

  uint8_t &zA = A;
  uint8_t &zX = X;
  uint8_t &zY = Y;
  uint8_t &zS = S;
  uint16_t &zPC = PC;
  uint16_t &zNextPC = NextPC;
  bool &zN = N;
  bool &zV = V;
  bool &zD = D;
  bool &zI = I;
  bool &zZ = Z;
  bool &zC = C;
  uint8_t &zIRQPending = IRQPending;
  uint8_t &zNMIPending = NMIPending;

  // SAIL runtime registers
  int64_t &zCycles = reinterpret_cast<int64_t &>(Cycles);
  bool z__monomorphizze_reads = false;  // SAIL memory model (unused in basic emulator)
  bool z__monomorphizze_writes = false; // SAIL memory model (unused in basic emulator)

  //===--------------------------------------------------------------------===//
  // SAIL-generated member variables (let bindings)
  //===--------------------------------------------------------------------===//

#define GET_EMULATOR_MEMBERS
#include "MOSGenEmulator.inc"
#undef GET_EMULATOR_MEMBERS

  //===--------------------------------------------------------------------===//
  // SAIL-generated helper functions (as class methods)
  //===--------------------------------------------------------------------===//

#define GET_EMULATOR_METHODS
#include "MOSGenEmulator.inc"
#undef GET_EMULATOR_METHODS

  /// Execute a single decoded instruction directly (for superoptimizer).
  /// This bypasses the normal fetch-decode-execute cycle.
  /// Note: Does not update PC or Cycles - caller is responsible.
  void executeInst(const MCInst &Inst);

private:
  const MCDisassembler *Disassembler;
  const MCInstrInfo *InstrInfo;

  /// Execute a single decoded instruction (internal implementation).
  /// Branches/jumps call set_next_pc() to override the default next PC.
  void execute(const MCInst &Inst);
};

} // namespace MOS
} // namespace llvm

#endif // LLVM_LIB_TARGET_MOS_MOSCONTEXT_H
