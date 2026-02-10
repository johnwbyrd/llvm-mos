//===-- llvm/Emulator/Context.h - CPU Execution Context --------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Abstract base class for CPU execution contexts.
///
/// A Context represents one CPU core with its own registers, PC, and cycle
/// counter. Subclasses implement step() to execute one instruction. Memory
/// access routes through the parent System, which handles device mapping,
/// watchpoints, and undo logging.
///
/// For reverse debugging, register writes must be logged. Subclasses should
/// use recordAndSet() instead of direct assignment, or override writeRegister
/// to call System::recordRegisterWrite() before modifying state.
///
/// SAIL integration: The z-prefixed methods (zreadMem, zwriteMem) are called
/// by SAIL-generated instruction implementations. SAIL is a formal ISA
/// specification language; the emulator backend compiles SAIL to C++ that
/// inherits from Context.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_CONTEXT_H
#define LLVM_EMULATOR_CONTEXT_H

#include "llvm/Emulator/Semihost/RiffCodec.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/TargetParser/Triple.h"
#include <cassert>
#include <cstdint>
#include <cstring>

namespace llvm {

class MCDisassembler;
class MCInstPrinter;
class MCSubtargetInfo;

namespace emu {

class System;
class TraceWriter;

/// Abstract base class for CPU execution contexts.
/// Each target architecture provides a concrete implementation.
class Context {
public:
  virtual ~Context() = default; //===-- llvm/Emulator/Trace.h - Execution Trace
                                //Interface ------*- C++ -*-===//
                                //
  // Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
  // See https://llvm.org/LICENSE.txt for license information.
  // SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
  //
  //===----------------------------------------------------------------------===//
  ///
  /// \file
  /// \brief Records CPU execution for debugging and analysis.
  ///
  /// Tracing captures every instruction executed, along with register and
  /// memory state, producing a complete record of program behavior. This is
  /// invaluable for debugging (comparing expected vs actual execution),
  /// validating the emulator against hardware or reference implementations, and
  /// understanding unfamiliar code.
  ///
  /// Three output formats are provided:
  /// - Text: human-readable, tab-separated for easy grepping
  /// - JSON Lines: machine-parseable, one object per line for streaming
  /// - VCD: IEEE 1364 waveform format for visualization in GTKWave
  ///
  //===----------------------------------------------------------------------===//

  //===--------------------------------------------------------------------===//
  // Execution Control
  //===--------------------------------------------------------------------===//

  /// Execute a single instruction and return true on success.
  virtual bool step() = 0;

  /// Execute instructions until halted or an error occurs.
  /// Returns true if execution completed normally (via halt/exit).
  virtual bool run();

  /// Execute for approximately the given number of cycles.
  /// Returns the actual number of cycles executed (may overshoot).
  virtual uint64_t executeFor(uint64_t Cycles);

  /// Reset the CPU to its initial state.
  virtual void reset() = 0;

  //===--------------------------------------------------------------------===//
  // State Accessors
  //===--------------------------------------------------------------------===//

  /// Get the current program counter.
  virtual uint64_t getPC() const = 0;

  /// Set the program counter.
  virtual void setPC(uint64_t PC) = 0;

  /// Get the total number of cycles executed.
  virtual uint64_t getCycles() const = 0;

  /// Check if the CPU has halted.
  virtual bool isHalted() const = 0;

  /// Request the CPU to halt with the given exit code.
  /// The halt takes effect before the next instruction executes.
  virtual void halt(int ExitCode = 0) = 0;

  /// Get the exit code (valid only when halted).
  virtual int getExitCode() const { return 0; }

  //===--------------------------------------------------------------------===//
  // Interrupt Support
  //===--------------------------------------------------------------------===//

  /// Assert the IRQ line (level-triggered).
  /// The CPU will check this before each instruction and vector if enabled.
  /// Default implementation does nothing (for CPUs without IRQ support).
  virtual void assertIRQ() {}

  /// Deassert the IRQ line.
  virtual void deassertIRQ() {}

  /// Assert NMI (edge-triggered).
  /// The CPU will handle this before the next instruction.
  virtual void assertNMI() {}

  /// Get the address bus width in bits.
  /// Derived from the target Triple's pointer width.
  /// Requires STI to be set via setSubtargetInfo().
  virtual unsigned getAddressBits() const {
    assert(STI && "Context requires SubtargetInfo to be set");
    return STI->getTargetTriple().getArchPointerBitWidth();
  }

  /// Get the platform configuration for semihosting.
  /// Derived from the target Triple (pointer size, endianness).
  /// IntSize defaults to PtrSize for semihosting protocol purposes.
  /// Requires STI to be set via setSubtargetInfo().
  virtual semihost::PlatformConfig getPlatformConfig() const {
    assert(STI && "Context requires SubtargetInfo to be set");
    const Triple &T = STI->getTargetTriple();
    uint8_t PtrSize = T.getArchPointerBitWidth() / 8;
    return semihost::PlatformConfig(
        PtrSize, // IntSize = PtrSize for semihosting
        PtrSize,
        T.isLittleEndian() ? llvm::endianness::little : llvm::endianness::big);
  }

  //===--------------------------------------------------------------------===//
  // System Integration
  //===--------------------------------------------------------------------===//

  /// Get the parent system (if any).
  System *getSystem() const { return Sys; }

  /// Set the parent system and context index (for undo journal).
  void setSystem(System *S, size_t Idx = 0) {
    Sys = S;
    ContextIndex = Idx;
  }

  /// Get the context index within the system.
  size_t getContextIndex() const { return ContextIndex; }

  //===--------------------------------------------------------------------===//
  // Register Access (per-CPU state)
  //===--------------------------------------------------------------------===//

  /// Get the number of registers in this CPU.
  /// Register metadata (names, DWARF numbers) comes from existing target info.
  virtual unsigned getNumRegisters() const { return 0; }

  /// Read a register value into the buffer.
  /// @param RegNum Register number (0..getNumRegisters()-1).
  /// @param Buf Buffer to receive the value.
  /// @param BufSize Size of buffer.
  /// @return true on success.
  virtual bool readRegister(unsigned RegNum, void *Buf, size_t BufSize) const {
    return false;
  }

  /// Write a register value from the buffer.
  /// @param RegNum Register number (0..getNumRegisters()-1).
  /// @param Buf Buffer containing the new value.
  /// @param BufSize Size of buffer.
  /// @return true on success.
  virtual bool writeRegister(unsigned RegNum, const void *Buf, size_t BufSize) {
    return false;
  }

  /// Write a register without logging to the undo journal.
  /// Used during checkpoint restore to avoid infinite log growth.
  virtual bool writeRegisterNoLog(unsigned RegNum, const void *Buf,
                                  size_t BufSize) {
    return writeRegister(RegNum, Buf, BufSize);
  }

  //===--------------------------------------------------------------------===//
  // Debugging Support
  //===--------------------------------------------------------------------===//

  /// Set an instruction printer for disassembly output.
  void setInstPrinter(MCInstPrinter *IP) { InstPrinter = IP; }

  /// Set the subtarget info for disassembly.
  void setSubtargetInfo(const MCSubtargetInfo *Info) { STI = Info; }

  /// Set a trace writer for execution tracing.
  void setTraceWriter(TraceWriter *TW) { Trace = TW; }

  /// Get the trace writer (if any).
  TraceWriter *getTraceWriter() const { return Trace; }

  /// Enable/disable instruction tracing (uses TraceWriter if set).
  void setTracing(bool Enable) { Tracing = Enable; }

  /// Check if tracing is enabled.
  bool isTracing() const { return Tracing; }

  //===--------------------------------------------------------------------===//
  // Memory Access (routes through System)
  //===--------------------------------------------------------------------===//

  uint8_t read(uint64_t Addr);
  void write(uint64_t Addr, uint8_t Value);

  // SAIL wrappers - called by generated instruction implementations
  uint8_t zreadMem(uint64_t Addr) { return read(Addr); }
  void zwriteMem(uint64_t Addr, uint8_t Value) { write(Addr, Value); }

protected:
  System *Sys = nullptr;
  size_t ContextIndex =
      0; ///< Index within the parent system (for undo journal)
  MCInstPrinter *InstPrinter = nullptr;
  const MCSubtargetInfo *STI = nullptr;
  TraceWriter *Trace = nullptr;
  bool Tracing = false;

  /// Helper for subclasses to record and set a register value.
  /// Records the old value to the undo journal, then updates the register.
  /// @param RegNum Register number for undo journal tracking.
  /// @param Reg Reference to the register variable.
  /// @param NewVal New value to set.
  template <typename T> void recordAndSet(unsigned RegNum, T &Reg, T NewVal);
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_CONTEXT_H
