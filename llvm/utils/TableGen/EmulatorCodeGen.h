//===- EmulatorCodeGen.h - C++ Code Generator for SAIL IR ------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the CodeGen class that emits C++ code from SAIL Jib IR.
//
// The generated code is organized into two sections controlled by preprocessor
// guards:
//
//   GET_SAIL_CLASS_TYPES - At namespace scope: type definitions (structs,
//                          enums, unions) and mcInstToSail() mapping function
//   GET_SAIL_CLASS_BODY  - Inside class definition: registers, pure virtual
//                          externals, swizzled wrappers, helper methods
//
// Usage:
//   // At namespace scope (before class definition)
//   #define GET_SAIL_CLASS_TYPES
//   #include "TargetGenEmulator.inc"
//   #undef GET_SAIL_CLASS_TYPES
//
//   // Inside a class definition
//   class TargetSail {
//   #define GET_SAIL_CLASS_BODY
//   #include "TargetGenEmulator.inc"
//   #undef GET_SAIL_CLASS_BODY
//   };
//
// Key Contract:
//   The SAIL IR must contain a function named "zexecute" and a union named
//   "zinstruction". These are the entry points for instruction execution.
//   The "z" prefix comes from SAIL's z-encoding of identifiers.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_UTILS_TABLEGEN_EMULATORCODEGEN_H
#define LLVM_UTILS_TABLEGEN_EMULATORCODEGEN_H

#include "EmulatorAST.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace llvm {
namespace emu {

//===----------------------------------------------------------------------===//
// Name Conversion Utilities
//===----------------------------------------------------------------------===//

/// Decode a SAIL z-encoded identifier.
///
/// SAIL encodes identifiers with a 'z' prefix and special character sequences.
/// For example: "zadd_bits" decodes to "add_bits", and "zAzB" contains
/// encoded special characters.
inline std::string decodeSailName(StringRef Encoded) {
  if (Encoded.empty() || Encoded[0] != 'z')
    return Encoded.str();

  std::string Decoded;
  Decoded.reserve(Encoded.size());

  for (size_t I = 1; I < Encoded.size(); ++I) {
    char C = Encoded[I];
    if (C != 'z') {
      Decoded += C;
      continue;
    }
    // Decode z-sequence based on SAIL's zencode.rs
    if (++I >= Encoded.size())
      break;
    C = Encoded[I];
    if (C >= '0' && C <= '9')
      Decoded += (C - '0') + ' ';      // '0'-'9' -> ' '-')'
    else if (C >= 'A' && C <= 'F')
      Decoded += (C - 'A') + '*';      // 'A'-'F' -> '*'-'/'
    else if (C >= 'G' && C <= 'M')
      Decoded += (C - 'G') + ':';      // 'G'-'M' -> ':'-'@'
    else if (C >= 'N' && C <= 'S')
      Decoded += (C - 'N') + '[';      // 'N'-'S' -> '['-'`'
    else if (C >= 'T' && C <= 'W')
      Decoded += (C - 'T') + '{';      // 'T'-'W' -> '{'-'~'
    else if (C == 'z')
      Decoded += 'z';
  }

  return Decoded;
}

/// Sanitize a decoded name for use as a C++ identifier.
inline std::string sanitizeForCpp(StringRef Decoded) {
  std::string Result;
  Result.reserve(Decoded.size() * 2);

  for (size_t I = 0; I < Decoded.size(); ++I) {
    char C = Decoded[I];
    if (isalnum(C)) {
      Result += C;
    } else if (C == '_') {
      Result += '_';
    } else if (C == '>') {
      // Check for -> (arrow)
      if (I > 0 && Decoded[I - 1] == '-') {
        if (!Result.empty() && Result.back() == '_')
          Result.pop_back();
        Result += "_to_";
      } else {
        Result += "_gt_";
      }
    } else if (C == '<') {
      Result += "_lt_";
    } else if (C == '=') {
      Result += "_eq_";
    } else if (C == '-') {
      // Don't add yet - might be part of ->
      if (I + 1 < Decoded.size() && Decoded[I + 1] == '>')
        continue;
      Result += '_';
    } else if (C == ' ' || C == '(' || C == ')') {
      // Skip whitespace and parens
    } else {
      Result += '_';
    }
  }

  // Clean up multiple underscores
  std::string Clean;
  for (char C : Result) {
    if (C == '_' && !Clean.empty() && Clean.back() == '_')
      continue;
    Clean += C;
  }

  // Trim leading/trailing underscores
  while (!Clean.empty() && Clean.back() == '_')
    Clean.pop_back();
  while (!Clean.empty() && Clean.front() == '_')
    Clean.erase(0, 1);

  return Clean;
}

/// Check if a name is a C++ reserved word and add prefix if needed.
inline std::string handleReservedWords(std::string Name) {
  if (Name.empty())
    return Name;

  // If starts with digit, prefix with tmp
  if (isdigit(Name[0]))
    Name = "tmp" + Name;

  // Check C++ reserved words
  static const StringSet<> Reserved = {
      "unsigned", "signed", "int",    "bool",     "char",   "void",
      "return",   "if",     "else",   "for",      "while",  "do",
      "switch",   "case",   "break",  "continue", "goto",   "default"};

  if (Reserved.contains(Name))
    return "_" + Name;

  return Name;
}

/// Convert a SAIL z-encoded identifier to a valid C++ identifier.
inline std::string toCppIdent(StringRef Encoded) {
  // Handle $-prefixed temporaries (e.g., $0, $1077)
  if (!Encoded.empty() && Encoded[0] == '$')
    return ("tmp" + Encoded.substr(1).str());

  // Not z-encoded
  if (Encoded.empty() || Encoded[0] != 'z')
    return Encoded.str();

  std::string Decoded = decodeSailName(Encoded);
  std::string Sanitized = sanitizeForCpp(Decoded);
  return handleReservedWords(Sanitized);
}

/// Normalize a variable name (handle $-prefixed temporaries).
inline std::string normalizeVarName(StringRef Name) {
  if (!Name.empty() && Name[0] == '$')
    return ("tmp" + Name.substr(1).str());
  return Name.str();
}

//===----------------------------------------------------------------------===//
// Primitive Implementation Lookup
//===----------------------------------------------------------------------===//

// Forward declaration - implementation below
inline StringRef getPrimitiveImpl(StringRef ExternalName);

/// Check if an external name is a primitive (implemented by codegen).
/// Primitives are not emitted as virtual functions for users to implement.
inline bool isPrimitive(StringRef ExternalName) {
  return !getPrimitiveImpl(ExternalName).empty();
}

/// Get C++ implementation for a SAIL primitive based on its external name.
/// Returns empty string if the primitive is unknown.
inline StringRef getPrimitiveImpl(StringRef ExternalName) {
  // Map of external primitive names to C++ implementations
  static const StringMap<StringRef> Primitives = {
      // Arithmetic
      {"add_bits", "return p0 + p1;"},
      {"add_bits_int", "return p0 + p1;"},
      {"sub_bits", "return p0 - p1;"},
      {"sub_bits_int", "return p0 - p1;"},

      // Bitwise
      {"and_bits", "return p0 & p1;"},
      {"or_bits", "return p0 | p1;"},
      {"xor_bits", "return p0 ^ p1;"},
      {"not_bits", "return ~p0;"},

      // Shifts
      {"shiftl", "return p0 << p1;"},
      {"shiftr", "return p0 >> p1;"},

      // Concatenation
      {"append", "return (p0 << 8) | p1;"},
      {"append_64", "(void)p0; return p1;"},

      // Comparison
      {"eq_bits", "return p0 == p1;"},
      {"neq_bits", "return p0 != p1;"},
      {"gteq", "return p0 >= p1;"},
      {"lteq", "return p0 <= p1;"},
      {"gt_int", "return p0 > p1;"},
      {"gt", "return p0 > p1;"},
      {"lt_int", "return p0 < p1;"},
      {"lt", "return p0 < p1;"},

      // Type conversions
      {"sail_unsigned", "return p0;"},
      {"unsigned", "return p0;"},
      {"sail_signed", "return (int64_t)p0;"},
      {"signed", "return (int64_t)p0;"},
      {"sign_extend", "return (int64_t)(int8_t)p0;"},

      // Bit extraction
      {"vector_subrange", "return (p0 >> p2) & ((1ULL << (p1 - p2 + 1)) - 1);"},

      // Boolean
      {"and_bool", "return p0 && p1;"},
      {"or_bool", "return p0 || p1;"},

      // Integer operations
      {"eq_int", "return p0 == p1;"},
      {"neq_int", "return p0 != p1;"},
      {"add_int", "return p0 + p1;"},
      {"sub_int", "return p0 - p1;"},
      {"mult_int", "return p0 * p1;"},
      {"neg_int", "return -p0;"},

      // Undefined/default values
      {"undefined_bitvector", "return 0;"},
      {"undefined_int", "return 0;"},
      {"undefined_bool", "return false;"},
      {"undefined_unit", "return;"},

      // Zero/sign extension
      {"zero_extend", "return p0;"},

      // Length (returns bit width - for now just return a constant)
      {"length", "return sizeof(p0) * 8;"},

      // Monomorphization (identity for concrete types)
      {"monomorphize", "return p0;"},

      // Cycle counting (no-op for basic emulator)
      {"cycle_count", "return;"},

      // Register reset (no-op - user should override)
      {"reset_registers", "return;"},
  };

  auto It = Primitives.find(ExternalName);
  if (It != Primitives.end())
    return It->second;

  // Type conversion patterns: %type->%type
  if (ExternalName.contains("->"))
    return "return p0;";

  return "";
}

//===----------------------------------------------------------------------===//
// CodeGen Class
//===----------------------------------------------------------------------===//

/// C++ code generator for SAIL Jib IR.
class CodeGen {
  const JibIR &IR;
  raw_ostream &OS;

  // Lookup maps built in constructor
  StringMap<const ValDecl *> ValsByName;
  StringMap<const UnionDef *> UnionsByName;
  StringSet<> UnionVariantNames;
  StringSet<> StructNames;

public:
  CodeGen(const JibIR &IR, raw_ostream &OS) : IR(IR), OS(OS) {
    // Build lookup maps
    for (const auto &Val : IR.Vals)
      ValsByName[Val.Name] = &Val;
    for (const auto &TypeDef : IR.Types) {
      if (auto *Union = std::get_if<UnionDef>(&TypeDef)) {
        UnionsByName[Union->Name] = Union;
        for (const auto &Variant : Union->Variants)
          UnionVariantNames.insert(Variant.first);
      } else if (auto *Struct = std::get_if<StructDef>(&TypeDef)) {
        StructNames.insert(Struct->Name);
      }
    }
  }

  /// Emit all generated code sections.
  void emit() {
    // Find required entry points
    const UnionDef *InstrUnion = nullptr;
    const FnDef *ExecuteFn = nullptr;

    for (const auto &TypeDef : IR.Types) {
      if (auto *Union = std::get_if<UnionDef>(&TypeDef)) {
        if (Union->Name == "zinstruction")
          InstrUnion = Union;
      }
    }
    for (const auto &Func : IR.Functions)
      if (Func.Name == "zexecute")
        ExecuteFn = &Func;

    if (!InstrUnion || !ExecuteFn) {
      OS << "// Error: SAIL IR must contain 'zinstruction' union and "
            "'zexecute' function\n";
      return;
    }

    size_t NumInstructions = InstrUnion->Variants.size();
    OS << "// Generated from SAIL Jib IR\n";
    OS << "// Instructions: " << NumInstructions << " from SAIL\n\n";

    // Types section (at namespace scope)
    OS << "#ifdef GET_SAIL_CLASS_TYPES\n";
    emitTypes(*InstrUnion);
    emitMapping(*InstrUnion);
    OS << "#endif // GET_SAIL_CLASS_TYPES\n\n";

    // Class body section (inside class definition)
    OS << "#ifdef GET_SAIL_CLASS_BODY\n";
    OS << "public:\n";
    emitMembers();
    emitExternals();
    OS << "protected:\n";
    emitWrappers();
    emitMethods();
    OS << "#endif // GET_SAIL_CLASS_BODY\n";
  }

private:
  //===--------------------------------------------------------------------===//
  // Type Emission
  //===--------------------------------------------------------------------===//

  void emitVariantStruct(StringRef Name, const Type &Ty) {
    if (Ty.Kind == Type::TK_Unit) {
      OS << "struct " << Name << " {};\n";
    } else {
      OS << "struct " << Name << " { " << Ty.toCpp() << " value; };\n";
    }
  }

  void emitTypes(const UnionDef &InstrUnion) {
    // Emit instruction variant structs
    OS << "// Instruction variant structs\n";
    for (const auto &Variant : InstrUnion.Variants)
      emitVariantStruct(Variant.first, Variant.second);
    OS << "\n";

    // Emit instruction union type
    OS << "// Instruction union type\n";
    OS << "using " << InstrUnion.Name << " = std::variant<";
    bool First = true;
    for (const auto &Variant : InstrUnion.Variants) {
      if (!First)
        OS << ", ";
      First = false;
      OS << Variant.first;
    }
    OS << ">;\n\n";

    // Emit other types in declaration order (preserves dependencies)
    for (const auto &TypeDef : IR.Types) {
      if (auto *Union = std::get_if<UnionDef>(&TypeDef)) {
        if (Union->Name == InstrUnion.Name)
          continue;
        OS << "// " << Union->Name << "\n";
        for (const auto &Variant : Union->Variants)
          emitVariantStruct(Variant.first, Variant.second);
        OS << "using " << Union->Name << " = std::variant<";
        First = true;
        for (const auto &Variant : Union->Variants) {
          if (!First)
            OS << ", ";
          First = false;
          OS << Variant.first;
        }
        OS << ">;\n\n";
      } else if (auto *Struct = std::get_if<StructDef>(&TypeDef)) {
        OS << "struct " << Struct->Name << " {\n";
        for (const auto &Field : Struct->Fields)
          OS << "  " << Field.second.toCppField() << " " << Field.first << ";\n";
        OS << "};\n\n";
      }
    }

    // Emit enums
    for (const auto &Enum : IR.Enums) {
      OS << "// " << Enum.Name << "\n";
      for (size_t I = 0; I < Enum.Variants.size(); ++I)
        OS << "static constexpr int " << Enum.Variants[I] << " = " << I
           << ";\n";
      OS << "\n";
    }
  }

  //===--------------------------------------------------------------------===//
  // Member Emission
  //===--------------------------------------------------------------------===//

  void emitMembers() {
    // Emit registers as member variables
    if (!IR.Registers.empty()) {
      OS << "// Registers from SAIL\n";
      for (const auto &Reg : IR.Registers)
        OS << Reg.Ty.toCpp() << " " << Reg.Name << " = {};\n";
      OS << "\n";
    }

    // Emit let bindings as member variables
    if (!IR.Lets.empty()) {
      OS << "// Let bindings (initialized by initLets())\n";
      for (const auto &Let : IR.Lets)
        OS << Let.Ty.toCpp() << " " << Let.Name << ";\n";
      OS << "\n";
    }
  }

  //===--------------------------------------------------------------------===//
  // External Function Emission
  //===--------------------------------------------------------------------===//

  /// Emit pure virtual functions for non-primitive externals.
  /// These are functions the user must implement (e.g., memory access).
  void emitExternals() {
    OS << "//===--------------------------------------------------------------------===//\n";
    OS << "// External Functions - implement these in derived class\n";
    OS << "//===--------------------------------------------------------------------===//\n\n";

    // Track functions with IR bodies (these are not externals)
    StringSet<> HasBody;
    for (const auto &Func : IR.Functions)
      HasBody.insert(Func.Name);

    // Track emitted externals to avoid duplicates
    StringSet<> Emitted;

    for (const auto &Val : IR.Vals) {
      // Skip if no external name, or if it's a primitive, or if it has a body
      if (Val.ExternalName.empty())
        continue;
      if (isPrimitive(Val.ExternalName))
        continue;
      if (HasBody.contains(Val.Name))
        continue;
      if (Emitted.contains(Val.ExternalName))
        continue;

      Emitted.insert(Val.ExternalName);

      // Emit comment with purpose
      OS << "// " << Val.ExternalName << "\n";

      // Emit pure virtual function with clean name
      OS << "virtual " << Val.ReturnType.toCpp() << " " << Val.ExternalName << "(";
      bool First = true;
      for (size_t I = 0; I < Val.ParamTypes.size(); ++I) {
        if (Val.ParamTypes[I].Kind == Type::TK_Unit)
          continue;
        if (!First)
          OS << ", ";
        First = false;
        OS << Val.ParamTypes[I].toCpp() << " p" << I;
      }
      OS << ") = 0;\n\n";
    }
  }

  /// Emit swizzled wrappers that delegate to clean-named virtual functions.
  /// Generated SAIL code calls these swizzled names; they forward to virtuals.
  void emitWrappers() {
    OS << "//===--------------------------------------------------------------------===//\n";
    OS << "// Swizzled wrappers - delegate to clean-named virtuals\n";
    OS << "//===--------------------------------------------------------------------===//\n\n";

    // Track functions with IR bodies (these are not externals)
    StringSet<> HasBody;
    for (const auto &Func : IR.Functions)
      HasBody.insert(Func.Name);

    // Track emitted wrappers to avoid duplicates
    StringSet<> Emitted;

    for (const auto &Val : IR.Vals) {
      // Skip if no external name, or if it's a primitive, or if it has a body
      if (Val.ExternalName.empty())
        continue;
      if (isPrimitive(Val.ExternalName))
        continue;
      if (HasBody.contains(Val.Name))
        continue;
      if (Emitted.contains(Val.Name))
        continue;

      Emitted.insert(Val.Name);

      // Emit wrapper that delegates to clean-named virtual
      OS << Val.ReturnType.toCpp() << " " << Val.Name << "(";
      bool First = true;
      for (size_t I = 0; I < Val.ParamTypes.size(); ++I) {
        if (Val.ParamTypes[I].Kind == Type::TK_Unit)
          continue;
        if (!First)
          OS << ", ";
        First = false;
        OS << Val.ParamTypes[I].toCpp() << " p" << I;
      }
      OS << ") {\n";

      // Call the clean-named virtual
      if (Val.ReturnType.Kind != Type::TK_Unit)
        OS << "  return ";
      else
        OS << "  ";
      OS << Val.ExternalName << "(";
      First = true;
      for (size_t I = 0; I < Val.ParamTypes.size(); ++I) {
        if (Val.ParamTypes[I].Kind == Type::TK_Unit)
          continue;
        if (!First)
          OS << ", ";
        First = false;
        OS << "p" << I;
      }
      OS << ");\n";
      OS << "}\n\n";
    }
  }

  //===--------------------------------------------------------------------===//
  // Method Emission
  //===--------------------------------------------------------------------===//

  void emitMethods() {
    OS << "// Helper methods from SAIL\n\n";

    // Track functions with IR bodies
    StringSet<> HasBody;
    for (const auto &Func : IR.Functions)
      HasBody.insert(Func.Name);

    // Track emitted functions to avoid duplicates
    StringSet<> Emitted;

    // Emit external primitives without IR bodies
    for (const auto &Val : IR.Vals) {
      if (Val.ExternalName.empty() || HasBody.contains(Val.Name))
        continue;
      if (Emitted.contains(Val.Name))
        continue; // Skip duplicates

      StringRef Impl = getPrimitiveImpl(Val.ExternalName);
      if (Impl.empty())
        continue;

      Emitted.insert(Val.Name);
      OS << Val.ReturnType.toCpp() << " " << Val.Name << "(";
      bool First = true;
      for (size_t I = 0; I < Val.ParamTypes.size(); ++I) {
        // Skip unit-typed parameters (void isn't valid as a parameter type)
        if (Val.ParamTypes[I].Kind == Type::TK_Unit)
          continue;
        if (!First)
          OS << ", ";
        First = false;
        OS << Val.ParamTypes[I].toCpp() << " p" << I;
      }
      OS << ") { " << Impl << " }\n";
    }
    OS << "\n";

    // Emit functions with IR bodies
    for (const auto &Func : IR.Functions) {
      emitFunction(Func);
    }

    // Emit initLets() to initialize let bindings
    if (!IR.Lets.empty()) {
      OS << "void initLets() {\n";
      for (const auto &Let : IR.Lets) {
        OS << "  {\n";
        emitInstrBody(Let.Body, "", "");
        OS << "  }\n";
      }
      OS << "}\n\n";
    }
  }

  void emitFunction(const FnDef &Func) {
    // Find return type and param types from val declarations
    Type ReturnType;
    ReturnType.Kind = Type::TK_Unit;
    std::vector<Type> ParamTypes;
    if (auto *Val = ValsByName.lookup(Func.Name)) {
      ReturnType = Val->ReturnType;
      ParamTypes = Val->ParamTypes;
    }

    // Emit signature
    OS << ReturnType.toCpp() << " " << Func.Name << "(";
    bool First = true;
    for (size_t I = 0; I < Func.ParamNames.size(); ++I) {
      Type ParamType =
          I < ParamTypes.size() ? ParamTypes[I] : Type{Type::TK_Bv, 64, ""};
      // Skip void/unit parameters
      if (ParamType.Kind == Type::TK_Unit)
        continue;
      if (!First)
        OS << ", ";
      First = false;
      OS << ParamType.toCpp() << " " << Func.ParamNames[I];
    }
    OS << ") {\n";

    emitInstrBody(Func.Body, "", "");

    OS << "}\n\n";
  }

  //===--------------------------------------------------------------------===//
  // Instruction Body Emission
  //
  // Uses a two-pass approach:
  //   Pass 1: Emit all variable declarations at the top
  //   Pass 2: Emit instructions with labels
  //
  // This is required because SAIL IR uses goto/labels, and C++ requires
  // that variables not be declared after a goto target label in the same
  // scope (otherwise the variable might be uninitialized).
  //===--------------------------------------------------------------------===//

  void emitInstrBody(const std::vector<Instr> &Body, StringRef ArgName,
                     StringRef InstrName) {
    // Collect jump targets for label emission
    DenseSet<int64_t> JumpTargets;
    for (const auto &Inst : Body) {
      if (Inst.Kind == Instr::IK_Jump || Inst.Kind == Instr::IK_Goto)
        JumpTargets.insert(Inst.Target);
    }

    // Track unit-type variables (these are not actually declared)
    StringSet<> UnitVars;
    // Track declared variables (to avoid re-declaring in Init)
    StringSet<> DeclaredVars;

    // Pass 1: Emit all declarations at the top
    for (const auto &Inst : Body) {
      if (Inst.Kind != Instr::IK_Decl && Inst.Kind != Instr::IK_Init)
        continue;
      std::string VarName = normalizeVarName(Inst.Name);
      if (Inst.Ty.Kind == Type::TK_Unit) {
        UnitVars.insert(VarName);
      } else if (!DeclaredVars.contains(VarName)) {
        OS << "  [[maybe_unused]] " << Inst.Ty.toCpp() << " " << VarName << ";\n";
        DeclaredVars.insert(VarName);
      }
    }

    // Pass 2: Emit instructions with labels
    for (const auto &Inst : Body) {
      if (JumpTargets.contains(Inst.LineNumber))
        OS << "L" << Inst.LineNumber << ":;\n";

      emitInstr(Inst, ArgName, InstrName, UnitVars);
    }
  }

  void emitInstr(const Instr &Inst, StringRef ArgName, StringRef InstrName,
                 const StringSet<> &UnitVars) {
    switch (Inst.Kind) {
    case Instr::IK_Decl:
      // Already emitted in pass 1
      break;

    case Instr::IK_Init: {
      std::string VarName = normalizeVarName(Inst.Name);
      std::string Value = emitExpr(Inst.Value, ArgName, InstrName);
      if (Inst.Ty.Kind == Type::TK_Unit) {
        if (!Value.empty() && Value != "{}")
          OS << "  " << Value << ";\n";
      } else if (!Value.empty()) {
        OS << "  " << VarName << " = " << Value << ";\n";
      }
      break;
    }

    case Instr::IK_Copy: {
      std::string VarName = normalizeVarName(Inst.Name);
      std::string Value = emitExpr(Inst.Value, ArgName, InstrName);
      bool IsInstrBody = !InstrName.empty();

      // Skip if value is a unit-type variable
      if (UnitVars.contains(Value))
        break;

      if (IsInstrBody && (VarName == "tmp0" || VarName == "return")) {
        // These are special result variables in instruction bodies
        if (!Value.empty() && Value != "{}")
          OS << "  " << Value << ";\n";
      } else if (UnitVars.contains(VarName)) {
        // Assigning to unit var - just execute RHS for side effects
        if (!Value.empty() && Value != "{}")
          OS << "  " << Value << ";\n";
      } else if (!Value.empty()) {
        OS << "  " << VarName << " = " << Value << ";\n";
      }
      break;
    }

    case Instr::IK_Jump:
      OS << "  if (" << emitExpr(Inst.Value, ArgName, InstrName) << ") goto L"
         << Inst.Target << ";\n";
      break;

    case Instr::IK_Goto:
      OS << "  goto L" << Inst.Target << ";\n";
      break;

    case Instr::IK_Return: {
      std::string Value = emitExpr(Inst.Value, ArgName, InstrName);
      // Return void if the value is unit (empty, {}, or a unit-typed variable)
      if (Value.empty() || Value == "{}" || UnitVars.contains(Value))
        OS << "  return;\n";
      else
        OS << "  return " << Value << ";\n";
      break;
    }

    case Instr::IK_End:
      break;
    }
  }

  //===--------------------------------------------------------------------===//
  // Expression Emission
  //===--------------------------------------------------------------------===//

  std::string emitExpr(const Expr &E, StringRef ArgName, StringRef InstrName) {
    switch (E.Kind) {
    case Expr::EK_Ident:
      return normalizeVarName(E.Text);

    case Expr::EK_Nat:
    case Expr::EK_Hex:
    case Expr::EK_Bin:
      return E.Text;

    case Expr::EK_True:
      return "true";

    case Expr::EK_False:
      return "false";

    case Expr::EK_Unit:
      return "{}";

    case Expr::EK_Op:
      return emitOperator(E, ArgName, InstrName);

    case Expr::EK_Call:
      return emitCall(E, ArgName, InstrName);

    case Expr::EK_Field:
      return emitExpr(E.Args[0], ArgName, InstrName) + "." + E.Text;

    case Expr::EK_Is:
      // SAIL's "is" (kind check) returns TRUE when the variant does NOT match.
      // See isla-lib/src/executor.rs: Val::Ctor(ctor_b, _) => Val::Bool(*ctor_a
      // != *ctor_b)
      return "!std::holds_alternative<" + E.Text + ">(" +
             emitExpr(E.Args[0], ArgName, InstrName) + ")";

    case Expr::EK_As:
      return "std::get<" + E.Text + ">(" +
             emitExpr(E.Args[0], ArgName, InstrName) + ").value";
    }
    return "";
  }

  std::string emitOperator(const Expr &E, StringRef ArgName,
                           StringRef InstrName) {
    StringRef OpName = E.OperatorName;
    const auto &Args = E.Args;

    // Binary operators
    if (Args.size() == 2) {
      std::string Left = emitExpr(Args[0], ArgName, InstrName);
      std::string Right = emitExpr(Args[1], ArgName, InstrName);

      // Lookup table for binary operators
      static const StringMap<const char *> BinaryOps = {
          {"bvadd", " + "},  {"bvsub", " - "},  {"bvand", " & "},
          {"bvor", " | "},   {"bvxor", " ^ "},  {"and", " && "},
          {"or", " || "},    {"eq", " == "},    {"neq", " != "},
          {"lt", " < "},     {"lteq", " <= "},  {"gt", " > "},
          {"gteq", " >= "},
      };

      auto It = BinaryOps.find(OpName);
      if (It != BinaryOps.end())
        return "(" + Left + It->second + Right + ")";

      if (OpName == "concat")
        return "((" + Left + " << 8) | " + Right + ")";

      if (OpName == "slice" && E.OperatorWidth > 0)
        return "((" + Left + " >> " + Right + ") & ((1 << " +
               std::to_string(E.OperatorWidth) + ") - 1))";
    }

    // Unary operators
    if (Args.size() == 1) {
      std::string Operand = emitExpr(Args[0], ArgName, InstrName);

      if (OpName == "bvnot")
        return "(~" + Operand + ")";
      if (OpName == "not")
        return "(!" + Operand + ")";
      if (OpName == "zero_extend")
        return Operand;
      if (OpName == "signed" && E.OperatorWidth > 0)
        return "(int" + std::to_string(E.OperatorWidth) + "_t)" + Operand;
      if (OpName == "unsigned" && E.OperatorWidth > 0)
        return "(uint" + std::to_string(E.OperatorWidth) + "_t)" + Operand;
    }

    // Unknown operator
    return "/* @" + E.OperatorName + " */";
  }

  std::string emitCall(const Expr &E, StringRef ArgName, StringRef InstrName) {
    std::string FnName = E.Text;
    bool IsUnionVariant = UnionVariantNames.contains(FnName);
    bool IsStruct = StructNames.contains(FnName);

    // Build argument list
    std::string ArgsStr;
    for (size_t I = 0; I < E.Args.size(); ++I) {
      if (I > 0)
        ArgsStr += ", ";
      std::string Arg = emitExpr(E.Args[I], ArgName, InstrName);
      // For structs, include {} for unit values to match field count.
      // For union variants and function calls, skip unit arguments.
      if (Arg != "{}" || IsStruct)
        ArgsStr += Arg;
    }

    // Union variants and structs use brace init, functions use parens
    if (IsUnionVariant || IsStruct)
      return FnName + "{" + ArgsStr + "}";
    return FnName + "(" + ArgsStr + ")";
  }

  //===--------------------------------------------------------------------===//
  // Mapping Function Emission
  //===--------------------------------------------------------------------===//

  void emitMapping(const UnionDef &InstrUnion) {
    OS << "// Map MCInst opcode to SAIL instruction variant\n";
    OS << "inline " << InstrUnion.Name << " mcInstToSail(const MCInst &Inst) {\n";
    OS << "  switch (Inst.getOpcode()) {\n";

    for (const auto &Variant : InstrUnion.Variants) {
      std::string OpcName = toCppIdent(Variant.first);
      OS << "  case " << OpcName << ":\n";
      if (Variant.second.Kind == Type::TK_Unit) {
        OS << "    return " << Variant.first << "{};\n";
      } else {
        std::string Cast = getCastForType(Variant.second);
        OS << "    return " << Variant.first << "{" << Cast
           << "(Inst.getOperand(0).getImm())};\n";
      }
    }

    OS << "  default:\n";
    OS << "    llvm_unreachable(\"Unknown opcode in mcInstToSail\");\n";
    OS << "  }\n";
    OS << "}\n";
  }

  std::string getCastForType(const Type &Ty) {
    switch (Ty.Kind) {
    case Type::TK_Bv:
      if (Ty.Width == 8)
        return "static_cast<uint8_t>";
      if (Ty.Width == 16)
        return "static_cast<uint16_t>";
      if (Ty.Width == 32)
        return "static_cast<uint32_t>";
      return "static_cast<uint64_t>";
    case Type::TK_Bool:
      return "static_cast<bool>";
    case Type::TK_I:
    case Type::TK_I64:
      return "static_cast<int64_t>";
    default:
      return "static_cast<uint64_t>";
    }
  }
};

} // namespace emu
} // namespace llvm

#endif // LLVM_UTILS_TABLEGEN_EMULATORCODEGEN_H
