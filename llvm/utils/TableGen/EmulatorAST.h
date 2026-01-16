//===- EmulatorAST.h - AST types for SAIL Jib IR ----------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Abstract Syntax Tree (AST) node types used to
// represent parsed SAIL Jib IR. The AST is produced by the Parser and
// consumed by the CodeGen to emit C++ code.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_UTILS_TABLEGEN_EMULATORAST_H
#define LLVM_UTILS_TABLEGEN_EMULATORAST_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace llvm {
namespace emu {

//===----------------------------------------------------------------------===//
// Type
//===----------------------------------------------------------------------===//

/// Represents a SAIL type.
struct Type {
  enum TypeKind { TK_I, TK_I64, TK_Bv, TK_Unit, TK_Bool, TK_Enum, TK_Struct };
  TypeKind Kind = TK_Bv;

  /// For Bv types: bit width (1, 8, 16, 32, 64). Zero means unspecified.
  int Width = 0;

  /// For Enum/Struct types: the type name.
  std::string Name;

  /// Convert this SAIL type to its C++ representation.
  std::string toCpp() const {
    switch (Kind) {
    case TK_I:
    case TK_I64:
      return "int64_t";
    case TK_Bv:
      if (Width == 1)
        return "uint8_t";
      if (Width == 8)
        return "uint8_t";
      if (Width == 16)
        return "uint16_t";
      if (Width == 32)
        return "uint32_t";
      return "uint64_t";
    case TK_Unit:
      return "void";
    case TK_Bool:
      return "bool";
    case TK_Enum:
      return "int";
    case TK_Struct:
      if (Name.empty())
        return "int";
      return Name;
    }
    return "uint64_t";
  }
};

//===----------------------------------------------------------------------===//
// Expression
//===----------------------------------------------------------------------===//

/// Represents a SAIL expression.
struct Expr {
  enum ExprKind {
    EK_Ident,  // Variable reference
    EK_Nat,    // Decimal literal
    EK_Hex,    // Hexadecimal literal
    EK_Bin,    // Binary literal
    EK_True,   // Boolean true
    EK_False,  // Boolean false
    EK_Unit,   // Unit value ()
    EK_Op,     // Operator application (@op)
    EK_Call,   // Function call
    EK_Field,  // Field access (.field)
    EK_Is,     // Variant check (is Constructor)
    EK_As      // Variant extraction (as Constructor)
  };
  ExprKind Kind = EK_Ident;

  /// Text content: identifier name, literal text, field name, or variant name.
  std::string Text;

  /// Numeric value for literal expressions.
  int64_t NumValue = 0;

  /// Arguments for Op/Call expressions, or base expression for Field/Is/As.
  std::vector<Expr> Args;

  /// For Op: the operator name (without @).
  std::string OperatorName;

  /// For Op with turbofish (@op::<N>): the width parameter.
  int OperatorWidth = 0;
};

//===----------------------------------------------------------------------===//
// Instruction
//===----------------------------------------------------------------------===//

/// Represents a SAIL Jib instruction.
struct Instr {
  enum InstrKind {
    IK_Decl,   // Variable declaration: name : Type
    IK_Init,   // Declaration with init: name : Type = Expr
    IK_Copy,   // Assignment: name = Expr
    IK_Jump,   // Conditional jump: jump Expr goto N
    IK_Goto,   // Unconditional jump: goto N
    IK_End,    // End of block
    IK_Return  // Return statement
  };
  InstrKind Kind = IK_Decl;

  /// Variable name for Decl/Init/Copy.
  std::string Name;

  /// Type for Decl/Init.
  Type Ty;

  /// RHS value for Init/Copy, or condition for Jump.
  Expr Value;

  /// Jump/Goto target (instruction index).
  int64_t Target = 0;

  /// Source line number within the function (used for label generation).
  int LineNumber = 0;
};

//===----------------------------------------------------------------------===//
// Top-Level Definitions
//===----------------------------------------------------------------------===//

/// Function definition.
struct FnDef {
  std::string Name;
  std::vector<std::string> ParamNames;
  std::vector<Type> ParamTypes;
  Type ReturnType;
  std::vector<Instr> Body;
};

/// External value declaration.
struct ValDecl {
  std::string Name;
  std::string ExternalName; // For external functions: the primitive name
  std::vector<Type> ParamTypes;
  Type ReturnType;
};

/// Enum definition.
struct EnumDef {
  std::string Name;
  std::vector<std::string> Variants;
};

/// Union (sum type) definition.
struct UnionDef {
  std::string Name;
  std::vector<std::pair<std::string, Type>> Variants;
};

/// Register definition.
struct RegisterDef {
  std::string Name;
  Type Ty;
};

/// Let binding (global constant initialized by code).
struct LetDef {
  std::string Name;
  Type Ty;
  std::vector<Instr> Body;
};

//===----------------------------------------------------------------------===//
// JibIR - Complete Parsed IR
//===----------------------------------------------------------------------===//

/// Container for all parsed SAIL Jib IR definitions.
struct JibIR {
  std::vector<RegisterDef> Registers;
  std::vector<EnumDef> Enums;
  std::vector<UnionDef> Unions;
  std::vector<ValDecl> Vals;
  std::vector<FnDef> Functions;
  std::vector<LetDef> Lets;
};

} // namespace emu
} // namespace llvm

#endif // LLVM_UTILS_TABLEGEN_EMULATORAST_H
