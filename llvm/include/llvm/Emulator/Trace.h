//===-- llvm/Emulator/Trace.h - Execution Trace Interface ------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the TraceWriter interface for recording execution traces.
// Different implementations support various output formats (text, VCD, etc.)
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EMULATOR_TRACE_H
#define LLVM_EMULATOR_TRACE_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>
#include <string>
#include <vector>

namespace llvm {

class MCInst;
class MCInstPrinter;
class MCSubtargetInfo;

namespace emu {

/// Register value for trace output.
struct TraceReg {
  StringRef Name;
  uint64_t Value;
  unsigned Width; // In bits
};

/// Abstract interface for trace output.
/// Implementations can write text, VCD, JSON, or other formats.
class TraceWriter {
public:
  virtual ~TraceWriter() = default;

  /// Called at start of trace.
  virtual void traceStart() {}

  /// Called at end of trace.
  virtual void traceEnd() {}

  /// Record an instruction execution.
  /// @param Cycle Current cycle count
  /// @param PC Program counter before execution
  /// @param Inst The instruction being executed
  /// @param Regs Register values before execution
  virtual void traceInstruction(uint64_t Cycle, uint64_t PC,
                                const MCInst &Inst,
                                ArrayRef<TraceReg> Regs) = 0;

  /// Record a memory read.
  virtual void traceMemRead(uint64_t Cycle, uint64_t Addr, uint64_t Value,
                            unsigned Width) {}

  /// Record a memory write.
  virtual void traceMemWrite(uint64_t Cycle, uint64_t Addr, uint64_t Value,
                             unsigned Width) {}
};

/// Simple text-based trace writer.
/// Output format (tab-separated, machine-parseable):
///   CYCLE\tPC\tA=XX X=XX Y=XX ...\tINSTRUCTION
class TextTraceWriter : public TraceWriter {
public:
  TextTraceWriter(raw_ostream &OS, MCInstPrinter *Printer = nullptr,
                  const MCSubtargetInfo *STI = nullptr)
      : OS(OS), Printer(Printer), STI(STI) {}

  void traceInstruction(uint64_t Cycle, uint64_t PC, const MCInst &Inst,
                        ArrayRef<TraceReg> Regs) override;

  void traceMemRead(uint64_t Cycle, uint64_t Addr, uint64_t Value,
                    unsigned Width) override;

  void traceMemWrite(uint64_t Cycle, uint64_t Addr, uint64_t Value,
                     unsigned Width) override;

private:
  raw_ostream &OS;
  MCInstPrinter *Printer;
  const MCSubtargetInfo *STI;
};

/// JSON Lines trace writer.
/// Each line is a self-contained JSON object for easy streaming/parsing.
/// Format: {"cycle":N,"pc":"XXXX","regs":{"A":"XX",...},"inst":"..."}
class JSONTraceWriter : public TraceWriter {
public:
  JSONTraceWriter(raw_ostream &OS, MCInstPrinter *Printer = nullptr,
                  const MCSubtargetInfo *STI = nullptr)
      : OS(OS), Printer(Printer), STI(STI) {}

  void traceInstruction(uint64_t Cycle, uint64_t PC, const MCInst &Inst,
                        ArrayRef<TraceReg> Regs) override;

  void traceMemRead(uint64_t Cycle, uint64_t Addr, uint64_t Value,
                    unsigned Width) override;

  void traceMemWrite(uint64_t Cycle, uint64_t Addr, uint64_t Value,
                     unsigned Width) override;

private:
  raw_ostream &OS;
  MCInstPrinter *Printer;
  const MCSubtargetInfo *STI;
};

/// VCD (Value Change Dump) trace writer.
/// IEEE 1364 standard format for waveform viewers like GTKWave.
/// Register definitions are learned dynamically from the first instruction.
class VCDTraceWriter : public TraceWriter {
public:
  VCDTraceWriter(raw_ostream &OS, unsigned PCWidth = 16)
      : OS(OS), PCWidth(PCWidth) {}

  void traceEnd() override;

  void traceInstruction(uint64_t Cycle, uint64_t PC, const MCInst &Inst,
                        ArrayRef<TraceReg> Regs) override;

  void traceMemRead(uint64_t Cycle, uint64_t Addr, uint64_t Value,
                    unsigned Width) override;

  void traceMemWrite(uint64_t Cycle, uint64_t Addr, uint64_t Value,
                     unsigned Width) override;

private:
  raw_ostream &OS;
  unsigned PCWidth;
  uint64_t LastCycle = ~0ULL;
  bool HeaderWritten = false;

  // Register info learned from first instruction
  struct RegInfo {
    std::string Name;
    unsigned Width;
    char VCDId;      // Single-char VCD identifier
    uint64_t PrevValue = ~0ULL;
  };
  std::vector<RegInfo> Registers;
  uint64_t PrevPC = ~0ULL;

  void writeHeader(ArrayRef<TraceReg> Regs);
  void emitTimestamp(uint64_t Cycle);
  void emitValue(char ID, uint64_t Value, unsigned Width);
};

} // namespace emu
} // namespace llvm

#endif // LLVM_EMULATOR_TRACE_H
