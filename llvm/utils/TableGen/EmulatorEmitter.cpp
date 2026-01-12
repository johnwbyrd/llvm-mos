//===- EmulatorEmitter.cpp - Generate instruction emulator ----------------===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// OVERVIEW
// ========
// This TableGen backend generates C++ switch cases for instruction emulation.
// It is completely architecture-agnostic - it knows nothing about specific
// instruction sets, addressing modes, or CPU architectures.
//
// INPUT FORMAT
// ============
// The backend processes Instruction records that have a non-empty "Emulate"
// field. It also supports EmulatorInst records for backward compatibility.
//
// The Emulate code can contain $Variable references. When the backend sees
// $Foo, it looks up "Foo" as a field on the same record. This lookup follows
// TableGen inheritance, so variables can come from parent classes (like
// AddressingMode providing $EA, $Value, $Base).
//
// Variables can reference other variables, creating a dependency chain.
// The backend resolves these dependencies and emits them in the correct order.
//
// For multi-line variable definitions:
//   - All lines except the last are emitted as setup statements
//   - The last line is treated as an expression and assigned to the variable
//
// Example TableGen input (DRY style - emulation in Instruction):
//
//   let Emulate = [{ A = $Value; setNZ(A); }] in {
//     defm LDA : Op<0xA9, "lda", Immediate>;  // EA/Value from Immediate mode
//     defm LDA : Op<0xA5, "lda", ZeroPage>;   // EA/Value from ZeroPage mode
//   }
//
// OUTPUT FORMAT
// =============
// The backend emits C++ switch cases wrapped in #ifdef GET_EMULATOR_CASES:
//
//   #ifdef GET_EMULATOR_CASES
//   case MOS::LDA_ZeroPage: {
//     auto EA = (uint16_t)Inst.getOperand(0).getImm();
//     auto Value = read(EA);
//     A = Value;
//     setNZ(A);
//     break;
//   }
//   #endif
//
// FEATURE PREDICATES
// ==================
// The backend reads the "Predicates" field from the Instruction.
// For each predicate that has a "PredicateName" field, it emits a runtime
// feature check:
//
//   if (!hasFeature(Namespace::FeatureName)) goto unhandled;
//
// This allows the emulator to reject instructions not valid for the current
// CPU configuration, even if they made it through the disassembler.
//
// This behavior is controlled by the -emulator-feature-checks flag:
//   -emulator-feature-checks=true   Emit checks on every instruction
//   -emulator-feature-checks=false  (default) Skip checks (trust disassembler)
//
// USAGE
// =====
// The generated code is meant to be included inside a switch statement:
//
//   bool MyEmulator::execute(const MCInst &Inst) {
//     switch (Inst.getOpcode()) {
//     #define GET_EMULATOR_CASES
//     #include "MyTargetEmulator.inc"
//     #undef GET_EMULATOR_CASES
//     default:
//     unhandled:
//       return handleUnknownInstruction(Inst);
//     }
//     return true;
//   }
//
// The including code must provide:
//   - Any functions/variables referenced in the Emulate code (read, write,
//     registers like A/X/Y, helper functions like setNZ, etc.)
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringSet.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"
#include <string>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "emulator-emitter"

static cl::opt<bool> EmitFeatureChecks(
    "emulator-feature-checks",
    cl::desc("Emit hasFeature() checks for each instruction (default: false)"),
    cl::init(false));

namespace {

class EmulatorEmitter {
  const RecordKeeper &Records;

public:
  EmulatorEmitter(const RecordKeeper &R) : Records(R) {}

  void run(raw_ostream &OS);

private:
  /// Find all $Variable references in code, skipping comments.
  StringSet<> findVariableRefs(StringRef Code);

  /// Look up a variable's code from the record, searching through all fields.
  StringRef lookupVariable(const Record *Rec, StringRef VarName);

  /// Emit code for a variable, resolving dependencies first.
  void emitVariable(raw_ostream &OS, StringRef VarName, const Record *Rec,
                    StringSet<> &Emitted);

  /// Substitute $Var with Var in code.
  std::string substituteVars(StringRef Code);

  /// Emit code for one Instruction (or EmulatorInst for backward compat).
  void emitInstructionCase(raw_ostream &OS, const Record *Inst);

  /// For EmulatorInst records, find the referenced Instruction.
  const Record *getReferencedInstruction(const Record *EmuInst);
};

} // end anonymous namespace

StringSet<> EmulatorEmitter::findVariableRefs(StringRef Code) {
  StringSet<> Refs;
  size_t Pos = 0;
  while (Pos < Code.size()) {
    // Skip // comments
    if (Pos + 1 < Code.size() && Code[Pos] == '/' && Code[Pos + 1] == '/') {
      while (Pos < Code.size() && Code[Pos] != '\n')
        ++Pos;
      continue;
    }
    // Skip /* */ comments
    if (Pos + 1 < Code.size() && Code[Pos] == '/' && Code[Pos + 1] == '*') {
      Pos += 2;
      while (Pos + 1 < Code.size() &&
             !(Code[Pos] == '*' && Code[Pos + 1] == '/'))
        ++Pos;
      Pos += 2;
      continue;
    }
    // Look for $Variable
    if (Code[Pos] == '$') {
      size_t Start = Pos + 1;
      size_t End = Start;
      // Variable names start with a letter or underscore
      if (End < Code.size() && (isalpha(Code[End]) || Code[End] == '_')) {
        while (End < Code.size() && (isalnum(Code[End]) || Code[End] == '_'))
          ++End;
        Refs.insert(Code.substr(Start, End - Start));
        Pos = End;
        continue;
      }
    }
    ++Pos;
  }
  return Refs;
}

std::string EmulatorEmitter::substituteVars(StringRef Code) {
  std::string Result;
  size_t Pos = 0;
  while (Pos < Code.size()) {
    if (Code[Pos] == '$') {
      // Extract variable name
      size_t Start = Pos + 1;
      size_t End = Start;
      while (End < Code.size() && (isalnum(Code[End]) || Code[End] == '_'))
        ++End;
      if (End > Start) {
        // Just use the variable name without the $
        Result += Code.substr(Start, End - Start);
        Pos = End;
        continue;
      }
    }
    Result += Code[Pos++];
  }
  return Result;
}

StringRef EmulatorEmitter::lookupVariable(const Record *Rec, StringRef VarName) {
  // Try to get this variable as a string field from the record
  // This will search through the record and all its superclasses
  if (Rec->isValueUnset(VarName))
    return StringRef();

  const RecordVal *RV = Rec->getValue(VarName);
  if (!RV)
    return StringRef();

  // Must be a code or string type
  if (const auto *SI = dyn_cast<StringInit>(RV->getValue()))
    return SI->getValue();

  return StringRef();
}

void EmulatorEmitter::emitVariable(raw_ostream &OS, StringRef VarName,
                                   const Record *Rec, StringSet<> &Emitted) {
  // Already emitted?
  if (Emitted.count(VarName))
    return;

  // Look up the variable's code from the record
  StringRef Code = lookupVariable(Rec, VarName);
  if (Code.empty() || Code.trim().empty()) {
    PrintFatalError(Rec->getLoc(),
                    "Variable $" + VarName + " referenced but not defined");
  }

  // First, emit any variables this one depends on
  StringSet<> Deps = findVariableRefs(Code);
  for (const auto &Dep : Deps) {
    emitVariable(OS, Dep.getKey(), Rec, Emitted);
  }

  // Now emit this variable's code
  std::string Substituted = substituteVars(Code.trim());

  // Split into lines - all but last are setup, last is the expression
  SmallVector<StringRef, 8> Lines;
  StringRef(Substituted).split(Lines, '\n', -1, false);

  // Remove empty lines
  SmallVector<StringRef, 8> NonEmpty;
  for (StringRef L : Lines) {
    if (!L.trim().empty())
      NonEmpty.push_back(L.trim());
  }

  if (NonEmpty.empty()) {
    PrintFatalError(Rec->getLoc(), "Variable $" + VarName + " has empty code");
  }

  // Emit setup lines as-is
  for (size_t i = 0; i + 1 < NonEmpty.size(); ++i) {
    OS << "    " << NonEmpty[i] << "\n";
  }

  // Last line is the expression - assign to variable
  OS << "    auto " << VarName << " = " << NonEmpty.back() << ";\n";

  Emitted.insert(VarName);
}

const Record *EmulatorEmitter::getReferencedInstruction(const Record *EmuInst) {
  // For EmulatorInst records, find the referenced Instruction
  for (const RecordVal &RV : EmuInst->getValues()) {
    if (const auto *DI = dyn_cast<DefInit>(RV.getValue())) {
      if (DI->getDef()->isSubClassOf("Instruction")) {
        return DI->getDef();
      }
    }
  }
  return nullptr;
}

void EmulatorEmitter::emitInstructionCase(raw_ostream &OS,
                                          const Record *Rec) {
  // Get the Emulate code - this is the ONLY thing we care about
  StringRef EmulateCode = lookupVariable(Rec, "Emulate");
  if (EmulateCode.empty() || EmulateCode.trim().empty()) {
    // No emulation code - skip this instruction, runtime handles it in default
    return;
  }

  // Determine if this is an Instruction record or an EmulatorInst record.
  // For Instruction: use Rec directly for both case label and field lookup.
  // For EmulatorInst: use the referenced Instruction for the case label,
  //                   but Rec for field lookup (EA/Value/Base/Emulate).
  const Record *Inst = Rec;
  if (Rec->isSubClassOf("EmulatorInst")) {
    Inst = getReferencedInstruction(Rec);
    if (!Inst) {
      // EmulatorInst without referenced instruction - skip
      return;
    }
  } else if (!Rec->isSubClassOf("Instruction")) {
    // Not an Instruction or EmulatorInst - skip
    return;
  }

  // Get the target namespace from the instruction
  StringRef Namespace = Inst->getValueAsString("Namespace");

  OS << "  case " << Namespace << "::" << Inst->getName() << ": {\n";

  // Check feature predicates - if instruction requires features not present,
  // fall through to default handler. Controlled by -emulator-feature-checks.
  if (EmitFeatureChecks) {
    std::vector<const Record *> Predicates =
        Inst->getValueAsListOfDefs("Predicates");
    for (const Record *Pred : Predicates) {
      // Get the feature name from PredicateName field if present
      if (!Pred->isValueUnset("PredicateName")) {
        StringRef FeatureName = Pred->getValueAsString("PredicateName");
        OS << "    if (!hasFeature(" << Namespace << "::" << FeatureName
           << ")) goto unhandled;\n";
      }
    }
  }

  // Find all variable references in the emulate code
  StringSet<> Refs = findVariableRefs(EmulateCode);
  StringSet<> Emitted;

  // Emit each referenced variable (with dependency resolution)
  // For EmulatorInst, look up variables on Rec (not Inst)
  for (const auto &Ref : Refs) {
    emitVariable(OS, Ref.getKey(), Rec, Emitted);
  }

  // Emit the emulate code with substitutions
  std::string Code = substituteVars(EmulateCode.trim());
  SmallVector<StringRef, 16> Lines;
  StringRef(Code).split(Lines, '\n', -1, false);
  for (StringRef Line : Lines) {
    StringRef Trimmed = Line.trim();
    if (!Trimmed.empty()) {
      OS << "    " << Trimmed << "\n";
    }
  }

  OS << "    break;\n";
  OS << "  }\n";
}

void EmulatorEmitter::run(raw_ostream &OS) {
  emitSourceFileHeader("Instruction Emulator", OS);

  // Collect records from both sources:
  // 1. Instruction records with non-empty Emulate field (new DRY style)
  // 2. EmulatorInst records (backward compatibility)
  std::vector<const Record *> RecordsToProcess;

  // Check all Instruction records for non-empty Emulate field
  // Only process instructions that actually have the Emulate field defined
  // (pseudo-instructions and logical instructions may not inherit from Inst)
  ArrayRef<const Record *> Instructions =
      Records.getAllDerivedDefinitions("Instruction");
  for (const Record *Inst : Instructions) {
    // Skip if the record doesn't have an Emulate field at all
    // (getValue returns nullptr if the field doesn't exist)
    const RecordVal *EmuField = Inst->getValue("Emulate");
    if (!EmuField)
      continue;
    StringRef EmulateCode = lookupVariable(Inst, "Emulate");
    if (!EmulateCode.empty() && !EmulateCode.trim().empty()) {
      RecordsToProcess.push_back(Inst);
    }
  }

  // Also include EmulatorInst records for backward compatibility
  ArrayRef<const Record *> EmuInsts =
      Records.getAllDerivedDefinitions("EmulatorInst");
  for (const Record *EmuInst : EmuInsts) {
    RecordsToProcess.push_back(EmuInst);
  }

  if (RecordsToProcess.empty()) {
    OS << "// No emulatable instructions found.\n";
    OS << "// Add 'let Emulate = [{ ... }]' to instruction definitions.\n";
    return;
  }

  OS << "// Generated instruction emulation switch cases.\n";
  OS << "// Include this file inside a switch(Inst.getOpcode()) block.\n";
  OS << "// Instructions: " << Instructions.size() << " total, "
     << (RecordsToProcess.size() - EmuInsts.size()) << " with Emulate field\n";
  OS << "// EmulatorInst: " << EmuInsts.size() << " (backward compat)\n\n";

  OS << "#ifdef GET_EMULATOR_CASES\n";

  for (const Record *Rec : RecordsToProcess) {
    emitInstructionCase(OS, Rec);
  }

  OS << "#endif // GET_EMULATOR_CASES\n";
}

static TableGen::Emitter::OptClass<EmulatorEmitter>
    X("gen-emulator", "Generate instruction emulator");
