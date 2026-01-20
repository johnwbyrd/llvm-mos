//===-- Enumerator.cpp - Superoptimizer sequence enumerator ---------------===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Target/MOS/Superopt/Superopt.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::superopt;

Enumerator::Enumerator(const MCInstrInfo &MII, const MCSubtargetInfo &STI,
                       const CostModel &CM, const EnumeratorConfig &Config)
    : MII(MII), STI(STI), CM(CM), Config(Config) {}

bool Enumerator::shouldPrune(const MCInst &Inst) const {
  if (!Config.EnablePruning || Current.empty())
    return false;

  const MCInst &Prev = Current.back();
  const MCInstrDesc &PrevDesc = MII.get(Prev.getOpcode());
  const MCInstrDesc &InstDesc = MII.get(Inst.getOpcode());

  // Rule 1: Identical consecutive implied-mode instructions (except shifts)
  // Most implied instructions are idempotent (CLC; CLC = CLC)
  if (Prev.getOpcode() == Inst.getOpcode() && InstDesc.getNumOperands() == 0) {
    // Check if it's a shift/rotate - those can be meaningfully repeated
    StringRef Name = MII.getName(Inst.getOpcode());
    if (!Name.starts_with("ASL") && !Name.starts_with("LSR") &&
        !Name.starts_with("ROL") && !Name.starts_with("ROR")) {
      return true;
    }
  }

  // Rule 2: Loading a register immediately after loading the same register
  // LDA #x; LDA #y -> LDA #y
  // This requires knowing which registers are defined - use MCInstrDesc
  // For now, use opcode name heuristics (MOS-specific, but works)
  StringRef PrevName = MII.getName(Prev.getOpcode());
  StringRef InstName = MII.getName(Inst.getOpcode());

  // If both start with "LDA" and are immediate mode, second kills first
  if (PrevName.starts_with("LDA_Immediate") &&
      InstName.starts_with("LDA_Immediate")) {
    return true;
  }
  if (PrevName.starts_with("LDX_Immediate") &&
      InstName.starts_with("LDX_Immediate")) {
    return true;
  }
  if (PrevName.starts_with("LDY_Immediate") &&
      InstName.starts_with("LDY_Immediate")) {
    return true;
  }

  // Rule 3: Transfer followed by overwriting load
  // TAX; LDX #y -> LDX #y (the TAX result is lost)
  if (PrevName == "TAX" && InstName.starts_with("LDX_Immediate")) {
    return true;
  }
  if (PrevName == "TAY" && InstName.starts_with("LDY_Immediate")) {
    return true;
  }
  if (PrevName == "TXA" && InstName.starts_with("LDA_Immediate")) {
    return true;
  }
  if (PrevName == "TYA" && InstName.starts_with("LDA_Immediate")) {
    return true;
  }

  // Rule 4: SEC followed by CLC (or vice versa)
  if ((PrevName == "SEC" && InstName == "CLC") ||
      (PrevName == "CLC" && InstName == "SEC")) {
    return true;
  }

  return false;
}

bool Enumerator::tryTemplate(const InstructionTemplate &Tmpl, unsigned Depth) {
  const MCInstrDesc &Desc = MII.get(Tmpl.Opcode);

  // Handle different operand configurations
  if (Tmpl.Operands.empty()) {
    // Implied addressing - no operands
    MCInst Inst;
    Inst.setOpcode(Tmpl.Opcode);

    if (shouldPrune(Inst)) {
      ++Stats.Pruned;
      return true;
    }

    Cost InstCost = CM.getInstructionCost(Inst);
    Cost NewCost = CurrentCost + InstCost;

    if (!(NewCost <= Config.MaxCost)) {
      ++Stats.CostPruned;
      return true;
    }

    Current.push_back(Inst);
    CurrentCost = NewCost;

    if (!enumerate(Depth + 1))
      return false;

    Current.pop_back();
    CurrentCost = CurrentCost + Cost(0, 0); // Recalculate would be cleaner
    // Actually, let's recalculate properly
    CurrentCost = CM.getSequenceCost(Current);
    return true;
  }

  // Single operand
  if (Tmpl.Operands.size() == 1) {
    const OperandTemplate &OpTmpl = Tmpl.Operands[0];

    auto tryWithImm = [&](uint64_t Imm) -> bool {
      MCInst Inst;
      Inst.setOpcode(Tmpl.Opcode);
      Inst.addOperand(MCOperand::createImm(Imm));

      if (shouldPrune(Inst)) {
        ++Stats.Pruned;
        return true;
      }

      Cost InstCost = CM.getInstructionCost(Inst);
      Cost NewCost = CurrentCost + InstCost;

      if (!(NewCost <= Config.MaxCost)) {
        ++Stats.CostPruned;
        return true;
      }

      Current.push_back(Inst);
      CurrentCost = NewCost;

      if (!enumerate(Depth + 1))
        return false;

      Current.pop_back();
      CurrentCost = CM.getSequenceCost(Current);
      return true;
    };

    switch (OpTmpl.K) {
    case OperandTemplate::None:
      llvm_unreachable("None operand in single-operand template");

    case OperandTemplate::Imm8:
      // Try all 256 values
      for (unsigned I = 0; I < 256; ++I) {
        if (!tryWithImm(I))
          return false;
      }
      break;

    case OperandTemplate::Imm8Subset:
      for (uint8_t V : OpTmpl.Values) {
        if (!tryWithImm(V))
          return false;
      }
      break;

    case OperandTemplate::Imm16:
      // 16-bit would be 64K values - need subset
      // For now, skip or use subset
      break;

    case OperandTemplate::ZeroPage:
      for (uint8_t V : OpTmpl.Values) {
        if (!tryWithImm(V))
          return false;
      }
      break;

    case OperandTemplate::Register:
      // Register operands handled by MCInst
      for (uint8_t V : OpTmpl.Values) {
        if (!tryWithImm(V))
          return false;
      }
      break;
    }
  }

  return true;
}

bool Enumerator::enumerate(unsigned Depth) {
  ++Stats.TotalGenerated;

  // Emit current sequence if non-empty
  if (!Current.empty()) {
    ++Stats.Emitted;
    if (!Callback(Current, CurrentCost))
      return false;
  }

  // Check depth limit
  if (Depth >= Config.MaxLength)
    return true;

  // Try each template
  for (const auto &Tmpl : Config.Templates) {
    if (!tryTemplate(Tmpl, Depth))
      return false;
  }

  return true;
}

void Enumerator::run(CandidateCallback CB) {
  Callback = std::move(CB);
  Stats = EnumeratorStats();
  Current.clear();
  CurrentCost = Cost();

  enumerate(0);

  if (Config.Verbose) {
    errs() << "Enumeration complete:\n"
           << "  Total states: " << Stats.TotalGenerated << "\n"
           << "  Pruned: " << Stats.Pruned << "\n"
           << "  Cost pruned: " << Stats.CostPruned << "\n"
           << "  Emitted: " << Stats.Emitted << "\n";
  }
}
