//===-- Trace.cpp - Execution Trace Implementation --------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Emulator/Trace.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/Support/Format.h"

using namespace llvm;
using namespace llvm::emu;

void TextTraceWriter::traceInstruction(uint64_t Cycle, uint64_t PC,
                                       const MCInst &Inst,
                                       ArrayRef<TraceReg> Regs) {
  // Format: CYCLE<tab>PC<tab>REGS<tab>DISASM
  // Example: 12345	$0200	A=42 X=00 Y=FF S=FD	lda #$42

  OS << Cycle << '\t';
  OS << format("$%04X", PC) << '\t';

  // Print registers
  bool First = true;
  for (const auto &R : Regs) {
    if (!First)
      OS << ' ';
    First = false;

    // Format based on register width
    if (R.Width <= 8)
      OS << R.Name << '=' << format("%02X", R.Value & 0xFF);
    else if (R.Width <= 16)
      OS << R.Name << '=' << format("%04X", R.Value & 0xFFFF);
    else
      OS << R.Name << '=' << format("%08X", R.Value & 0xFFFFFFFF);
  }

  OS << '\t';

  // Print disassembly if printer available
  if (Printer && STI) {
    Printer->printInst(&Inst, 0, "", *STI, OS);
  } else {
    OS << "<opcode " << Inst.getOpcode() << ">";
  }

  OS << '\n';
}

void TextTraceWriter::traceMemRead(uint64_t Cycle, uint64_t Addr,
                                   uint64_t Value, unsigned Width) {
  // Optional: memory access tracing
  // Format: CYCLE<tab>R<tab>ADDR<tab>VALUE
  // OS << Cycle << "\tR\t" << format("$%04X", Addr)
  //    << '\t' << format("%02X", Value) << '\n';
}

void TextTraceWriter::traceMemWrite(uint64_t Cycle, uint64_t Addr,
                                    uint64_t Value, unsigned Width) {
  // Optional: memory access tracing
  // Format: CYCLE<tab>W<tab>ADDR<tab>VALUE
  // OS << Cycle << "\tW\t" << format("$%04X", Addr)
  //    << '\t' << format("%02X", Value) << '\n';
}

//===----------------------------------------------------------------------===//
// JSONTraceWriter
//===----------------------------------------------------------------------===//

/// Escape a string for JSON output.
static void writeJSONString(raw_ostream &OS, StringRef S) {
  OS << '"';
  for (char C : S) {
    switch (C) {
    case '"': OS << "\\\""; break;
    case '\\': OS << "\\\\"; break;
    case '\n': OS << "\\n"; break;
    case '\r': OS << "\\r"; break;
    case '\t': OS << "\\t"; break;
    default:
      if (C >= 0x20 && C < 0x7F)
        OS << C;
      else
        OS << format("\\u%04X", (unsigned char)C);
    }
  }
  OS << '"';
}

void JSONTraceWriter::traceInstruction(uint64_t Cycle, uint64_t PC,
                                       const MCInst &Inst,
                                       ArrayRef<TraceReg> Regs) {
  OS << "{\"cycle\":" << Cycle;
  OS << ",\"pc\":\"" << format("%04X", PC) << "\"";

  // Registers as object
  OS << ",\"regs\":{";
  bool First = true;
  for (const auto &R : Regs) {
    if (!First)
      OS << ',';
    First = false;
    OS << '"' << R.Name << "\":\"";
    if (R.Width <= 8)
      OS << format("%02X", R.Value & 0xFF);
    else if (R.Width <= 16)
      OS << format("%04X", R.Value & 0xFFFF);
    else
      OS << format("%08X", R.Value & 0xFFFFFFFF);
    OS << '"';
  }
  OS << '}';

  // Disassembly
  OS << ",\"inst\":";
  if (Printer && STI) {
    std::string InstStr;
    raw_string_ostream InstOS(InstStr);
    Printer->printInst(&Inst, 0, "", *STI, InstOS);
    // Trim leading/trailing whitespace
    StringRef Trimmed = StringRef(InstStr).trim();
    writeJSONString(OS, Trimmed);
  } else {
    OS << "\"opcode " << Inst.getOpcode() << "\"";
  }

  OS << "}\n";
}

void JSONTraceWriter::traceMemRead(uint64_t Cycle, uint64_t Addr,
                                   uint64_t Value, unsigned Width) {
  OS << "{\"cycle\":" << Cycle;
  OS << ",\"type\":\"read\"";
  OS << ",\"addr\":\"" << format("%04X", Addr) << "\"";
  OS << ",\"value\":\"" << format("%02X", Value & 0xFF) << "\"";
  OS << "}\n";
}

void JSONTraceWriter::traceMemWrite(uint64_t Cycle, uint64_t Addr,
                                    uint64_t Value, unsigned Width) {
  OS << "{\"cycle\":" << Cycle;
  OS << ",\"type\":\"write\"";
  OS << ",\"addr\":\"" << format("%04X", Addr) << "\"";
  OS << ",\"value\":\"" << format("%02X", Value & 0xFF) << "\"";
  OS << "}\n";
}

//===----------------------------------------------------------------------===//
// VCDTraceWriter
//===----------------------------------------------------------------------===//

void VCDTraceWriter::writeHeader(ArrayRef<TraceReg> Regs) {
  // VCD header - written on first instruction when we know the register set
  OS << "$version LLVM Emulator Trace $end\n";
  OS << "$timescale 1ns $end\n"; // 1 cycle = 1ns for simplicity
  OS << "$scope module cpu $end\n";

  // PC always gets ID '!'
  OS << "$var wire " << PCWidth << " ! PC $end\n";

  // Assign VCD IDs starting from '"' (ASCII 34)
  // VCD allows printable ASCII chars 33-126 except '$'
  char NextID = '"';
  for (const auto &R : Regs) {
    // Skip '$' which has special meaning in VCD
    if (NextID == '$')
      NextID++;

    RegInfo Info;
    Info.Name = R.Name.str();
    Info.Width = R.Width;
    Info.VCDId = NextID++;
    Info.PrevValue = ~0ULL;
    Registers.push_back(Info);

    OS << "$var wire " << R.Width << " " << Info.VCDId << " "
       << Info.Name << " $end\n";
  }

  OS << "$upscope $end\n";
  OS << "$enddefinitions $end\n";

  // Initial values (all X = unknown)
  OS << "$dumpvars\n";
  OS << 'b';
  for (unsigned I = 0; I < PCWidth; ++I)
    OS << 'x';
  OS << " !\n";

  for (const auto &Info : Registers) {
    OS << 'b';
    for (unsigned I = 0; I < Info.Width; ++I)
      OS << 'x';
    OS << ' ' << Info.VCDId << '\n';
  }
  OS << "$end\n";

  HeaderWritten = true;
}

void VCDTraceWriter::traceEnd() {
  // VCD doesn't require a footer
}

void VCDTraceWriter::emitTimestamp(uint64_t Cycle) {
  if (Cycle != LastCycle) {
    OS << '#' << Cycle << '\n';
    LastCycle = Cycle;
  }
}

void VCDTraceWriter::emitValue(char ID, uint64_t Value, unsigned Width) {
  OS << 'b';
  // Output bits MSB first
  for (int Bit = Width - 1; Bit >= 0; --Bit)
    OS << ((Value >> Bit) & 1);
  OS << ' ' << ID << '\n';
}

void VCDTraceWriter::traceInstruction(uint64_t Cycle, uint64_t PC,
                                      const MCInst &Inst,
                                      ArrayRef<TraceReg> Regs) {
  // Write header on first instruction when we know the register set
  if (!HeaderWritten)
    writeHeader(Regs);

  emitTimestamp(Cycle);

  // Emit PC if changed
  if (PC != PrevPC) {
    emitValue('!', PC, PCWidth);
    PrevPC = PC;
  }

  // Emit registers that changed
  for (size_t I = 0; I < Registers.size() && I < Regs.size(); ++I) {
    if (Regs[I].Value != Registers[I].PrevValue) {
      emitValue(Registers[I].VCDId, Regs[I].Value, Registers[I].Width);
      Registers[I].PrevValue = Regs[I].Value;
    }
  }
}

void VCDTraceWriter::traceMemRead(uint64_t Cycle, uint64_t Addr,
                                  uint64_t Value, unsigned Width) {
  // VCD is primarily for signal changes, memory access would need
  // separate signals defined. Skip for now.
}

void VCDTraceWriter::traceMemWrite(uint64_t Cycle, uint64_t Addr,
                                   uint64_t Value, unsigned Width) {
  // Same as above
}
