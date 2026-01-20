//===-- MOSSuperopt.cpp - MOS-specific superoptimizer implementation ------===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MOS-specific cost model and configuration for the superoptimizer.
//
//===----------------------------------------------------------------------===//

#include "llvm/Target/MOS/Superopt/Superopt.h"
#include "llvm/MC/MCInstrDesc.h"

using namespace llvm;
using namespace llvm::superopt;

namespace {

/// MOS cost model using TSFlags.
class MOSCostModel : public CostModel {
  const MCInstrInfo &MII;
  const MCSubtargetInfo &STI;

public:
  MOSCostModel(const MCInstrInfo &MII, const MCSubtargetInfo &STI)
      : MII(MII), STI(STI) {}

  Cost getInstructionCost(const MCInst &Inst) const override {
    const MCInstrDesc &Desc = MII.get(Inst.getOpcode());
    uint64_t TSFlags = Desc.TSFlags;

    // Extract byte count from instruction size
    unsigned Bytes = Desc.getSize();
    if (Bytes == 0) {
      // Estimate based on operand count for variable-size instructions
      Bytes = 1 + Desc.getNumOperands();
    }

    // Extract cycle count from TSFlags (bits 4-8)
    unsigned Cycles = MOS::getCyclesFromTSFlags(TSFlags);
    if (Cycles == 0) {
      // Default to 2 cycles if not specified
      Cycles = 2;
    }

    return Cost(Bytes, Cycles);
  }
};

/// Helper to look up opcode by name.
/// Returns 0 if not found.
unsigned lookupOpcode(const MCInstrInfo &MII, StringRef Name) {
  for (unsigned I = 0; I < MII.getNumOpcodes(); ++I) {
    if (MII.getName(I) == Name)
      return I;
  }
  return 0;
}

} // anonymous namespace

std::unique_ptr<CostModel>
superopt::MOS::createCostModel(const MCInstrInfo &MII,
                                const MCSubtargetInfo &STI) {
  return std::make_unique<MOSCostModel>(MII, STI);
}

EnumeratorConfig superopt::MOS::getRegisterOnlyConfig(const MCInstrInfo &MII) {
  EnumeratorConfig Config;
  Config.MaxLength = 5;
  Config.MaxCost = Cost(10, 20);

  // Common immediate values for synthesis
  SmallVector<uint8_t, 16> CommonImms = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x07, 0x08, 0x0F,
      0x10, 0x1F, 0x20, 0x3F, 0x40, 0x7F, 0x80, 0xFE, 0xFF};

  // Look up opcodes by name (portable across builds)
  auto addImplied = [&](StringRef Name) {
    if (unsigned Op = lookupOpcode(MII, Name))
      Config.Templates.push_back(InstructionTemplate(Op));
  };

  auto addImm8 = [&](StringRef Name) {
    if (unsigned Op = lookupOpcode(MII, Name))
      Config.Templates.push_back(
          InstructionTemplate(Op, OperandTemplate::imm8Subset(CommonImms)));
  };

  auto addImm8Full = [&](StringRef Name) {
    if (unsigned Op = lookupOpcode(MII, Name))
      Config.Templates.push_back(InstructionTemplate(Op, OperandTemplate::imm8()));
  };

  // Arithmetic (immediate)
  addImm8("ADC_Immediate");
  addImm8("SBC_Immediate");

  // Logical (immediate)
  addImm8("AND_Immediate");
  addImm8("ORA_Immediate");
  addImm8("EOR_Immediate");

  // Shifts (accumulator)
  addImplied("ASL_Accumulator");
  addImplied("LSR_Accumulator");
  addImplied("ROL_Accumulator");
  addImplied("ROR_Accumulator");

  // Transfers
  addImplied("TAX_Implied");
  addImplied("TXA_Implied");
  addImplied("TAY_Implied");
  addImplied("TYA_Implied");

  // Load immediate
  addImm8("LDA_Immediate");
  addImm8("LDX_Immediate");
  addImm8("LDY_Immediate");

  // Increment/Decrement
  addImplied("INX_Implied");
  addImplied("DEX_Implied");
  addImplied("INY_Implied");
  addImplied("DEY_Implied");

  // Flags
  addImplied("CLC_Implied");
  addImplied("SEC_Implied");
  addImplied("CLV_Implied");

  // Stack operations (base 6502 only)
  // Note: PHX/PLX/PHY/PLY are 65C02 and not supported by the SAIL emulator
  addImplied("PHA_Implied");
  addImplied("PLA_Implied");

  // Zero-page memory operations (needed for swaps and complex operations)
  // Use a small set of temp addresses to keep search space manageable
  SmallVector<uint8_t, 4> TempAddrs = {0x00, 0x01, 0x02};
  auto addZP = [&](StringRef Name) {
    if (unsigned Op = lookupOpcode(MII, Name))
      Config.Templates.push_back(
          InstructionTemplate(Op, OperandTemplate::imm8Subset(TempAddrs)));
  };
  addZP("STA_ZeroPage");
  addZP("STX_ZeroPage");
  addZP("STY_ZeroPage");
  addZP("LDA_ZeroPage");
  addZP("LDX_ZeroPage");
  addZP("LDY_ZeroPage");

  return Config;
}

EnumeratorConfig superopt::MOS::getArithmeticConfig(const MCInstrInfo &MII) {
  EnumeratorConfig Config;
  Config.MaxLength = 6;
  Config.MaxCost = Cost(12, 24);

  auto addImplied = [&](StringRef Name) {
    if (unsigned Op = lookupOpcode(MII, Name))
      Config.Templates.push_back(InstructionTemplate(Op));
  };

  auto addImm8Full = [&](StringRef Name) {
    if (unsigned Op = lookupOpcode(MII, Name))
      Config.Templates.push_back(InstructionTemplate(Op, OperandTemplate::imm8()));
  };

  // For arithmetic synthesis, try all immediate values
  addImm8Full("ADC_Immediate");
  addImm8Full("SBC_Immediate");
  addImm8Full("AND_Immediate");
  addImm8Full("ORA_Immediate");
  addImm8Full("EOR_Immediate");
  addImm8Full("LDA_Immediate");

  // Shifts
  addImplied("ASL_Accumulator");
  addImplied("LSR_Accumulator");
  addImplied("ROL_Accumulator");
  addImplied("ROR_Accumulator");

  // Carry manipulation (essential for ADC/SBC)
  addImplied("CLC_Implied");
  addImplied("SEC_Implied");

  return Config;
}

EnumeratorConfig superopt::MOS::getShiftConfig(const MCInstrInfo &MII) {
  EnumeratorConfig Config;
  Config.MaxLength = 8; // Shifts often need many instructions
  Config.MaxCost = Cost(16, 32);

  SmallVector<uint8_t, 8> ShiftImms = {0x00, 0x01, 0x80, 0xFF};

  auto addImplied = [&](StringRef Name) {
    if (unsigned Op = lookupOpcode(MII, Name))
      Config.Templates.push_back(InstructionTemplate(Op));
  };

  auto addImm8 = [&](StringRef Name, ArrayRef<uint8_t> Imms) {
    if (unsigned Op = lookupOpcode(MII, Name))
      Config.Templates.push_back(
          InstructionTemplate(Op, OperandTemplate::imm8Subset(Imms)));
  };

  // Shifts
  addImplied("ASL_Accumulator");
  addImplied("LSR_Accumulator");
  addImplied("ROL_Accumulator");
  addImplied("ROR_Accumulator");

  // Carry manipulation
  addImplied("CLC_Implied");
  addImplied("SEC_Implied");

  // ADC for combining shifted values
  addImm8("ADC_Immediate", ShiftImms);

  // AND/ORA for masking
  addImm8("AND_Immediate", ShiftImms);
  addImm8("ORA_Immediate", ShiftImms);

  return Config;
}
