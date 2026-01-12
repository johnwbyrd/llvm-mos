//===-- llvm/Emulator/Context.h - CPU Execution Context --------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the abstract base class for CPU execution contexts.
// Each Context represents one CPU with its own registers and program counter.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_CONTEXT_H
#define LLVM_EMULATOR_CONTEXT_H

#include "llvm/MC/MCInst.h"
#include <cstdint>

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
  virtual ~Context() = default;

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
  // System Integration
  //===--------------------------------------------------------------------===//

  /// Get the parent system (if any).
  System *getSystem() const { return Sys; }

  /// Set the parent system.
  void setSystem(System *S) { Sys = S; }

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

  /// Read a byte from the given address.
  uint8_t read(uint64_t Addr);

  /// Write a byte to the given address.
  void write(uint64_t Addr, uint8_t Value);

  /// Read a 16-bit little-endian value from the given address.
  uint16_t read16(uint64_t Addr) {
    return read(Addr) | (read(Addr + 1) << 8);
  }

  /// Write a 16-bit little-endian value to the given address.
  void write16(uint64_t Addr, uint16_t Value) {
    write(Addr, Value & 0xFF);
    write(Addr + 1, Value >> 8);
  }

protected:
  System *Sys = nullptr;
  MCInstPrinter *InstPrinter = nullptr;
  const MCSubtargetInfo *STI = nullptr;
  TraceWriter *Trace = nullptr;
  bool Tracing = false;
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_CONTEXT_H
