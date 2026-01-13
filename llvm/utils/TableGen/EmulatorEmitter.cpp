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

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
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

static cl::opt<std::string>
    SailIRFile("sail-ir",
               cl::desc("Path to SAIL-generated Jib IR file for instruction "
                        "semantics (optional)"),
               cl::init(""));

namespace {

//===----------------------------------------------------------------------===//
// Z-encoding decoder (from SAIL's zencode.rs)
//===----------------------------------------------------------------------===//

/// Decode a z-encoded identifier back to its original form.
/// SAIL uses z-encoding to represent all ASCII characters using only
/// C-identifier-safe characters. The scheme prefixes all names with 'z'
/// and encodes special characters as 'zX' where X maps to the original.
static std::string zdecode(StringRef Input) {
  if (Input.empty() || Input[0] != 'z')
    return Input.str();

  std::string Output;
  Output.reserve(Input.size());
  bool NextEncoded = false;

  for (size_t I = 1; I < Input.size(); ++I) {
    char C = Input[I];
    if (NextEncoded) {
      // Decode the character based on zencode.rs logic
      if (C <= '9') // '0'-'9' -> ' '-')' (ASCII 32-41)
        Output.push_back(C - 16);
      else if (C <= 'F') // 'A'-'F' -> '*'-'/' (ASCII 42-47)
        Output.push_back(C - 23);
      else if (C <= 'M') // 'G'-'M' -> ':'-'@' (ASCII 58-64)
        Output.push_back(C - 13);
      else if (C <= 'S') // 'N'-'S' -> '['-'`' (ASCII 91-96)
        Output.push_back(C + 13);
      else if (C == 'z') // 'z' -> 'z'
        Output.push_back('z');
      else // 'T'-'W' -> '{'-'~' (ASCII 123-126)
        Output.push_back(C + 39);
      NextEncoded = false;
    } else if (C == 'z') {
      NextEncoded = true;
    } else {
      Output.push_back(C);
    }
  }
  return Output;
}

//===----------------------------------------------------------------------===//
// Jib IR Parser
//===----------------------------------------------------------------------===//

/// Represents a parsed instruction body from the zexecute function.
/// Contains the raw IR lines that implement the instruction semantics.
struct JibInstructionBody {
  std::string Name;              // Decoded instruction name (e.g., "LDA_imm")
  std::string OperandType;       // Type of operand (%bv8, %bv16, %unit)
  std::vector<std::string> Body; // IR lines for this instruction
};

/// Represents a parsed function definition from the IR.
struct JibFunction {
  std::string Name;                // Decoded function name
  std::vector<std::string> Params; // Parameter names (decoded)
  std::vector<std::string> Body;   // IR lines for the function body
};

/// Parsed Jib IR file containing instruction semantics.
struct JibIR {
  StringMap<std::string> Registers; // register name -> type
  StringMap<JibInstructionBody> Instructions; // decoded name -> body
  StringMap<std::string> ExternalFunctions; // z-encoded name -> external name
  StringMap<JibFunction> Functions; // decoded name -> function definition

  bool empty() const { return Instructions.empty(); }
};

/// Parse a Jib IR file and extract instruction bodies from zexecute.
static JibIR parseJibIR(StringRef Path) {
  JibIR IR;

  // Read the file
  ErrorOr<std::unique_ptr<MemoryBuffer>> BufOrErr =
      MemoryBuffer::getFile(Path);
  if (!BufOrErr) {
    PrintError("Failed to open SAIL IR file: " + Path);
    return IR;
  }

  StringRef Content = (*BufOrErr)->getBuffer();
  SmallVector<StringRef, 1024> Lines;
  Content.split(Lines, '\n');

  // First pass: collect register definitions, external functions, and find zexecute
  size_t ExecuteStart = 0;
  for (size_t I = 0; I < Lines.size(); ++I) {
    StringRef Line = Lines[I].trim();

    // Parse register definitions: "register zName : %type"
    if (Line.starts_with("register ")) {
      StringRef Rest = Line.drop_front(9).trim();
      size_t ColonPos = Rest.find(':');
      if (ColonPos != StringRef::npos) {
        StringRef Name = Rest.substr(0, ColonPos).trim();
        StringRef Type = Rest.substr(ColonPos + 1).trim();
        std::string DecodedName = zdecode(Name);
        IR.Registers[DecodedName] = Type.str();
      }
    }

    // Parse external function declarations: "val zfoo = "external_name" : ..."
    // Format: val <z-encoded-name> = "<external-name>" : (types) -> type
    if (Line.starts_with("val ") && Line.contains(" = \"")) {
      StringRef Rest = Line.drop_front(4).trim();
      size_t EqPos = Rest.find(" = ");
      if (EqPos != StringRef::npos) {
        StringRef ZEncodedName = Rest.substr(0, EqPos).trim();
        StringRef AfterEq = Rest.substr(EqPos + 3).trim();
        // Extract the quoted external name
        if (AfterEq.starts_with("\"")) {
          size_t EndQuote = AfterEq.find('"', 1);
          if (EndQuote != StringRef::npos) {
            StringRef ExternalName = AfterEq.substr(1, EndQuote - 1);
            IR.ExternalFunctions[ZEncodedName] = ExternalName.str();
          }
        }
      }
    }

    // Find the start of fn zexecute
    if (Line.starts_with("fn zexecute(")) {
      ExecuteStart = I;
    }

    // Parse function definitions: "fn zname(params) {"
    // Skip zexecute (handled separately) and zmain/zinitializze_registers
    if (Line.starts_with("fn z") && !Line.starts_with("fn zexecute") &&
        !Line.starts_with("fn zmain") && !Line.starts_with("fn zinitializze")) {
      size_t ParenPos = Line.find('(');
      size_t CloseParenPos = Line.find(')');
      if (ParenPos != StringRef::npos && CloseParenPos != StringRef::npos) {
        StringRef FuncName = Line.substr(3, ParenPos - 3); // Skip "fn "
        StringRef ParamsStr = Line.substr(ParenPos + 1, CloseParenPos - ParenPos - 1);

        JibFunction Func;
        Func.Name = zdecode(FuncName);

        // Parse parameters
        SmallVector<StringRef, 4> Params;
        ParamsStr.split(Params, ',');
        for (StringRef P : Params) {
          P = P.trim();
          if (!P.empty())
            Func.Params.push_back(zdecode(P));
        }

        // Collect function body until "end;" or "}"
        for (size_t J = I + 1; J < Lines.size(); ++J) {
          StringRef BodyLine = Lines[J].trim();
          if (BodyLine == "end;" || BodyLine == "}") {
            break;
          }
          Func.Body.push_back(BodyLine.str());
        }

        IR.Functions[Func.Name] = std::move(Func);
      }
    }
  }

  if (ExecuteStart == 0) {
    PrintError("Could not find zexecute function in SAIL IR file");
    return IR;
  }

  // Second pass: parse the zexecute function body
  // The structure is:
  //   jump zmergez3var is zINSTR_variant goto N  <- marks start of instruction
  //   ...                                         <- instruction body
  //   goto END                                    <- marks end of instruction
  //   jump zmergez3var is zNEXT_variant goto M   <- next instruction

  std::string CurrentInstr;
  std::string CurrentOperandType;
  std::vector<std::string> CurrentBody;
  bool InExecute = false;

  for (size_t I = ExecuteStart + 1; I < Lines.size(); ++I) {
    StringRef Line = Lines[I].trim();

    // End of function
    if (Line == "end" || Line == "}") {
      // Save last instruction if any
      if (!CurrentInstr.empty() && !CurrentBody.empty()) {
        JibInstructionBody Body;
        Body.Name = CurrentInstr;
        Body.OperandType = CurrentOperandType;
        Body.Body = std::move(CurrentBody);
        IR.Instructions[CurrentInstr] = std::move(Body);
      }
      break;
    }

    // Check for instruction start: "jump zmergez3var is zINSTR goto N"
    if (Line.starts_with("jump ") && Line.contains(" is ") &&
        Line.contains(" goto ")) {
      // Save previous instruction
      if (!CurrentInstr.empty() && !CurrentBody.empty()) {
        JibInstructionBody Body;
        Body.Name = CurrentInstr;
        Body.OperandType = CurrentOperandType;
        Body.Body = std::move(CurrentBody);
        IR.Instructions[CurrentInstr] = std::move(Body);
      }

      // Extract instruction name: "jump zmergez3var is zINSTR goto N"
      size_t IsPos = Line.find(" is ");
      size_t GotoPos = Line.find(" goto ");
      if (IsPos != StringRef::npos && GotoPos != StringRef::npos) {
        StringRef EncodedName = Line.substr(IsPos + 4, GotoPos - IsPos - 4);
        CurrentInstr = zdecode(EncodedName);
        CurrentBody.clear();
        CurrentOperandType.clear();
        InExecute = true;
      }
      continue;
    }

    // Collect instruction body lines
    if (InExecute && !CurrentInstr.empty()) {
      // Skip unconditional gotos (they just mark end of instruction block)
      if (Line.starts_with("goto "))
        continue;

      // Extract operand type from "zmergez3var as zINSTR" lines
      if (Line.contains(" as ") && CurrentOperandType.empty()) {
        // The type comes from the union definition, but we can infer from usage
        // For now, just collect the body
      }

      CurrentBody.push_back(Line.str());
    }
  }

  return IR;
}

// Forward declaration
static std::string translateJibExp(StringRef Exp, const JibIR &IR);

/// Sanitize a Jib IR variable name for C++.
/// Decodes z-encoding and gives temporaries meaningful prefixes.
static std::string sanitizeVarName(StringRef Name) {
  std::string Decoded = zdecode(Name);
  // SAIL temporaries start with $ (e.g., $1077) - give them a meaningful prefix
  if (!Decoded.empty() && Decoded[0] == '$')
    return "tmp" + Decoded.substr(1);
  return Decoded;
}

/// Check if this is an internal/uninteresting assignment we should skip.
/// Returns 0 if should not skip, 1 if should skip entirely, 2 if should emit
/// expression for side effects but not assign.
static int shouldSkipAssignment(StringRef Loc, StringRef Exp) {
  std::string DecodedLoc = sanitizeVarName(Loc);
  // Skip unit assignments (exp = ())
  if (Exp.trim() == "()")
    return 1;
  // Skip assignments to internal return value ($0 -> tmp0, or literal "return")
  // But if the expression is a function call, emit it for side effects
  if (DecodedLoc == "tmp0" || Loc.trim() == "return") {
    // Check if Exp looks like a function call (contains '(' and isn't a cast)
    if (Exp.contains('(') && !Exp.starts_with("("))
      return 2; // Emit for side effects
    return 1;   // Skip entirely
  }
  return 0;
}

/// Translate a Jib IR instruction body to C++ code.
/// This performs the mechanical translation from Jib IR to C++.
static std::string translateJibToC(const JibInstructionBody &Instr,
                                   const JibIR &IR) {
  std::string Result;
  raw_string_ostream OS(Result);

  // Track unit-type variables so we can skip assignments to them
  StringSet<> UnitVars;

  for (const std::string &Line : Instr.Body) {
    StringRef L = StringRef(Line).trim();

    // Skip empty lines
    if (L.empty())
      continue;

    // Declaration: "id : %type"
    if (L.contains(" : %") && !L.contains(" = ")) {
      size_t ColonPos = L.find(" : ");
      if (ColonPos != StringRef::npos) {
        StringRef VarName = L.substr(0, ColonPos);
        StringRef Type = L.substr(ColonPos + 3);
        // Remove source location suffix if present
        size_t BacktickPos = Type.find('`');
        if (BacktickPos != StringRef::npos)
          Type = Type.substr(0, BacktickPos).trim();

        // Skip unit type declarations but remember them
        if (Type.starts_with("%unit")) {
          UnitVars.insert(sanitizeVarName(VarName));
          continue;
        }

        std::string SanitizedVar = sanitizeVarName(VarName);
        // Skip internal variables
        if (SanitizedVar == "tmp0")
          continue;

        std::string CppType = "uint8_t"; // Default
        if (Type.starts_with("%bv16") || Type.starts_with("%i16"))
          CppType = "uint16_t";
        else if (Type.starts_with("%bv32") || Type.starts_with("%i") ||
                 Type.starts_with("%i64"))
          CppType = "uint32_t";
        else if (Type.starts_with("%bool"))
          CppType = "bool";
        else if (Type.starts_with("%enum"))
          continue; // Skip enum declarations (like ExecutionResult)

        OS << "    " << CppType << " " << SanitizedVar << ";\n";
        continue;
      }
    }

    // Init: "id : %type = exp"
    if (L.contains(" : %") && L.contains(" = ")) {
      size_t ColonPos = L.find(" : ");
      size_t EqPos = L.find(" = ");
      if (ColonPos != StringRef::npos && EqPos != StringRef::npos) {
        StringRef VarName = L.substr(0, ColonPos);
        StringRef Type = L.substr(ColonPos + 3, EqPos - ColonPos - 3).trim();
        StringRef Exp = L.substr(EqPos + 3);

        // Remove source location suffix
        size_t BacktickPos = Exp.find('`');
        if (BacktickPos != StringRef::npos)
          Exp = Exp.substr(0, BacktickPos).trim();

        // Skip unit type initializations and internal vars
        if (Type.starts_with("%unit")) {
          UnitVars.insert(sanitizeVarName(VarName));
          continue;
        }
        if (Type.starts_with("%enum"))
          continue;
        int SkipResult = shouldSkipAssignment(VarName, Exp);
        if (SkipResult == 1)
          continue;

        std::string SanitizedVar = sanitizeVarName(VarName);
        std::string CppExp = translateJibExp(Exp, IR);

        // Skip if expression translates to nothing useful
        if (CppExp.empty() || CppExp == "{}")
          continue;

        if (SkipResult == 2) {
          // Emit for side effects only
          OS << "    " << CppExp << ";\n";
        } else {
          OS << "    auto " << SanitizedVar << " = " << CppExp << ";\n";
        }
        continue;
      }
    }

    // Assignment: "loc = exp"
    if (L.contains(" = ") && !L.contains(" : ")) {
      size_t EqPos = L.find(" = ");
      if (EqPos != StringRef::npos) {
        StringRef Loc = L.substr(0, EqPos);
        StringRef Exp = L.substr(EqPos + 3);

        // Remove source location suffix
        size_t BacktickPos = Exp.find('`');
        if (BacktickPos != StringRef::npos)
          Exp = Exp.substr(0, BacktickPos).trim();

        // Skip uninteresting assignments
        int SkipResult = shouldSkipAssignment(Loc, Exp);
        if (SkipResult == 1)
          continue;

        std::string SanitizedLoc = sanitizeVarName(Loc);
        std::string CppExp = translateJibExp(Exp, IR);

        // Skip if expression translates to nothing useful
        if (CppExp.empty() || CppExp == "{}")
          continue;

        // Emit for side effects only (e.g., function call whose return value is discarded)
        if (SkipResult == 2) {
          OS << "    " << CppExp << ";\n";
          continue;
        }

        // For unit-type variables, emit the expression as a statement (for side effects)
        // but don't assign it to anything
        if (UnitVars.count(SanitizedLoc)) {
          OS << "    " << CppExp << ";\n";
          continue;
        }

        OS << "    " << SanitizedLoc << " = " << CppExp << ";\n";
        continue;
      }
    }

    // Skip lines we don't understand yet
    OS << "    // TODO: " << L << "\n";
  }

  return Result;
}

/// Translate a Jib IR expression to C++.
static std::string translateJibExp(StringRef Exp, const JibIR &IR) {
  Exp = Exp.trim();

  // Boolean literals
  if (Exp == "true")
    return "true";
  if (Exp == "false")
    return "false";

  // Unit
  if (Exp == "()")
    return "{}";

  // Hex bitvector literal: 0xNNN
  if (Exp.starts_with("0x"))
    return Exp.str();

  // Binary bitvector literal: 0bNNN
  if (Exp.starts_with("0b"))
    return Exp.str();

  // Integer literal: N
  if (!Exp.empty() && (isdigit(Exp[0]) || Exp[0] == '-'))
    return Exp.str();

  // Builtin operations: @op(args)
  if (Exp.starts_with("@")) {
    size_t ParenPos = Exp.find('(');
    if (ParenPos != StringRef::npos) {
      StringRef Op = Exp.substr(1, ParenPos - 1);
      StringRef Args = Exp.substr(ParenPos + 1).drop_back(); // remove ')'

      // Handle turbofish syntax: @op::<N>(args)
      size_t TurboPos = Op.find("::<");
      std::string Width;
      if (TurboPos != StringRef::npos) {
        size_t EndPos = Op.find('>');
        Width = Op.substr(TurboPos + 3, EndPos - TurboPos - 3).str();
        Op = Op.substr(0, TurboPos);
      }

      // Parse args (simple comma-split for now)
      SmallVector<StringRef, 4> ArgList;
      Args.split(ArgList, ',');
      for (auto &A : ArgList)
        A = A.trim();

      // Translate operations
      if (Op == "bvadd" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " + " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "bvsub" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " - " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "bvand" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " & " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "bvor" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " | " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "bvxor" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " ^ " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "bvnot" && ArgList.size() == 1)
        return "(~" + translateJibExp(ArgList[0], IR) + ")";
      if (Op == "not" && ArgList.size() == 1)
        return "(!" + translateJibExp(ArgList[0], IR) + ")";
      if (Op == "and" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " && " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "or" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " || " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "eq" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " == " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "neq" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " != " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "lt" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " < " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "lteq" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " <= " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "gt" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " > " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "gteq" && ArgList.size() == 2)
        return "(" + translateJibExp(ArgList[0], IR) + " >= " +
               translateJibExp(ArgList[1], IR) + ")";
      if (Op == "concat" && ArgList.size() == 2) {
        // Need to know widths for shift amount - default to 8
        return "((" + translateJibExp(ArgList[0], IR) + " << 8) | " +
               translateJibExp(ArgList[1], IR) + ")";
      }
      if (Op == "slice" && !Width.empty() && ArgList.size() == 2) {
        // @slice::<N>(val, start) extracts N bits starting at start
        return "((" + translateJibExp(ArgList[0], IR) + " >> " +
               translateJibExp(ArgList[1], IR) + ") & ((1 << " + Width +
               ") - 1))";
      }
      if (Op == "zero_extend" && !Width.empty() && ArgList.size() == 1) {
        // Zero extension is a no-op in C++ with unsigned types
        return translateJibExp(ArgList[0], IR);
      }
      if (Op == "signed" && !Width.empty() && ArgList.size() == 1) {
        return "(int" + Width + "_t)" + translateJibExp(ArgList[0], IR);
      }
      if (Op == "unsigned" && !Width.empty() && ArgList.size() == 1) {
        return "(uint" + Width + "_t)" + translateJibExp(ArgList[0], IR);
      }

      // Unknown op - return as-is with comment
      return "/* @" + Op.str() + " */ " + Exp.str();
    }
  }

  // Function call: id(args) or $id(args)
  if (Exp.contains('(') && Exp.ends_with(')')) {
    bool External = Exp.starts_with("$");
    size_t ParenPos = Exp.find('(');
    StringRef FuncName = Exp.substr(External ? 1 : 0, ParenPos - (External ? 1 : 0));
    StringRef Args = Exp.substr(ParenPos + 1).drop_back();

    std::string DecodedFunc = zdecode(FuncName);

    // Handle SAIL type conversion functions like %i64->%i, %bv->%i
    // These just extract the value
    if (DecodedFunc.find("->") != std::string::npos ||
        StringRef(DecodedFunc).starts_with("%")) {
      // Type conversion - just return the argument
      SmallVector<StringRef, 4> ArgList;
      Args.split(ArgList, ',');
      if (ArgList.size() == 1)
        return translateJibExp(ArgList[0].trim(), IR);
      // Multiple args - just return first for now
      if (!ArgList.empty())
        return translateJibExp(ArgList[0].trim(), IR);
      return "0";
    }

    // Parse arguments first
    SmallVector<StringRef, 4> ArgList;
    Args.split(ArgList, ',');
    std::vector<std::string> TransArgs;
    for (StringRef Arg : ArgList) {
      Arg = Arg.trim();
      if (Arg == "()")
        continue;
      std::string TransArg = translateJibExp(Arg, IR);
      if (TransArg == "{}")
        continue;
      TransArgs.push_back(TransArg);
    }

    // Check if this is an external function from val declarations
    auto ExtIt = IR.ExternalFunctions.find(FuncName);
    if (ExtIt != IR.ExternalFunctions.end()) {
      StringRef ExtName = ExtIt->second;

      // Map SAIL library functions to C++ operators/expressions
      if (TransArgs.size() == 2) {
        if (ExtName == "add_bits" || ExtName == "add_bits_int")
          return "(" + TransArgs[0] + " + " + TransArgs[1] + ")";
        if (ExtName == "sub_bits" || ExtName == "sub_bits_int")
          return "(" + TransArgs[0] + " - " + TransArgs[1] + ")";
        if (ExtName == "and_bits")
          return "(" + TransArgs[0] + " & " + TransArgs[1] + ")";
        if (ExtName == "or_bits")
          return "(" + TransArgs[0] + " | " + TransArgs[1] + ")";
        if (ExtName == "xor_bits")
          return "(" + TransArgs[0] + " ^ " + TransArgs[1] + ")";
        if (ExtName == "shiftl")
          return "(" + TransArgs[0] + " << " + TransArgs[1] + ")";
        if (ExtName == "shiftr")
          return "(" + TransArgs[0] + " >> " + TransArgs[1] + ")";
        if (ExtName == "append")
          return "(((uint16_t)" + TransArgs[0] + " << 8) | " + TransArgs[1] + ")";
        if (ExtName == "eq_bits")
          return "(" + TransArgs[0] + " == " + TransArgs[1] + ")";
        if (ExtName == "neq_bits")
          return "(" + TransArgs[0] + " != " + TransArgs[1] + ")";
        if (ExtName == "gteq")
          return "(" + TransArgs[0] + " >= " + TransArgs[1] + ")";
        if (ExtName == "lteq")
          return "(" + TransArgs[0] + " <= " + TransArgs[1] + ")";
        if (ExtName == "gt_int")
          return "(" + TransArgs[0] + " > " + TransArgs[1] + ")";
        if (ExtName == "lt_int")
          return "(" + TransArgs[0] + " < " + TransArgs[1] + ")";
      }
      if (TransArgs.size() == 1) {
        if (ExtName == "sail_unsigned" || ExtName == "unsigned")
          return TransArgs[0]; // Already unsigned in C++
        if (ExtName == "sail_signed" || ExtName == "signed")
          return "(int8_t)(" + TransArgs[0] + ")";
        if (ExtName == "not_bits")
          return "(~" + TransArgs[0] + ")";
      }
      if (TransArgs.size() == 3) {
        // vector_subrange(bv, hi, lo) -> extract bits
        if (ExtName == "vector_subrange")
          return "((" + TransArgs[0] + " >> " + TransArgs[2] + ") & ((1 << (" +
                 TransArgs[1] + " - " + TransArgs[2] + " + 1)) - 1))";
      }

      // For unrecognized external functions, use the external name
      DecodedFunc = ExtName.str();
    }

    // Join translated args
    std::string ArgsStr;
    for (size_t I = 0; I < TransArgs.size(); ++I) {
      if (I > 0)
        ArgsStr += ", ";
      ArgsStr += TransArgs[I];
    }

    return DecodedFunc + "(" + ArgsStr + ")";
  }

  // Field access: exp.field
  if (Exp.contains('.')) {
    size_t DotPos = Exp.rfind('.');
    StringRef Base = Exp.substr(0, DotPos);
    StringRef Field = Exp.substr(DotPos + 1);
    return translateJibExp(Base, IR) + "." + zdecode(Field);
  }

  // Union unwrap: exp as variant
  if (Exp.contains(" as ")) {
    size_t AsPos = Exp.find(" as ");
    StringRef Base = Exp.substr(0, AsPos);
    // StringRef Variant = Exp.substr(AsPos + 4);
    // For instruction operand extraction, this is just getting the operand
    std::string DecodedBase = zdecode(Base);
    // If this is the merged argument, it's the instruction operand
    if (DecodedBase == "mergez3var" || StringRef(DecodedBase).contains("merge"))
      return "Inst.getOperand(0).getImm()";
    return translateJibExp(Base, IR);
  }

  // Union check: exp is variant
  if (Exp.contains(" is ")) {
    size_t IsPos = Exp.find(" is ");
    StringRef Base = Exp.substr(0, IsPos);
    StringRef Variant = Exp.substr(IsPos + 4);
    return "(" + translateJibExp(Base, IR) + ".is<" + zdecode(Variant) + ">())";
  }

  // Simple identifier - sanitize it (handles $-prefixed temps)
  std::string Sanitized = sanitizeVarName(Exp);

  return Sanitized;
}

//===----------------------------------------------------------------------===//
// EmulatorEmitter class
//===----------------------------------------------------------------------===//

class EmulatorEmitter {
  const RecordKeeper &Records;
  JibIR SailIR; // Parsed SAIL IR (if provided)

public:
  EmulatorEmitter(const RecordKeeper &R) : Records(R) {
    // Load SAIL IR if specified
    if (!SailIRFile.empty()) {
      SailIR = parseJibIR(SailIRFile);
      if (!SailIR.empty()) {
        errs() << "Loaded " << SailIR.Instructions.size()
               << " instruction semantics, " << SailIR.Functions.size()
               << " helper functions from SAIL IR\n";
      }
    }
  }

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

  /// Emit code for one Instruction (from Emulate field or SAIL IR).
  void emitInstructionCase(raw_ostream &OS, const Record *Inst);

  /// Try to find SAIL semantics for an instruction.
  /// Returns the C++ code if found, empty string otherwise.
  std::string getSailSemantics(StringRef InstName);
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
  const RecordVal *RV = Rec->getValue(VarName);
  if (!RV || !RV->getValue())
    return StringRef();

  // Check if value is unset
  if (isa<UnsetInit>(RV->getValue()))
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

std::string EmulatorEmitter::getSailSemantics(StringRef InstName) {
  if (SailIR.empty())
    return "";

  // Exact match only - SAIL instruction names must match TableGen names exactly
  auto It = SailIR.Instructions.find(InstName);
  if (It != SailIR.Instructions.end()) {
    return translateJibToC(It->second, SailIR);
  }

  return "";
}

void EmulatorEmitter::emitInstructionCase(raw_ostream &OS,
                                          const Record *Inst) {
  StringRef EmulateCode = lookupVariable(Inst, "Emulate");
  std::string SailCode;

  // Check if we have SAIL semantics for this instruction
  if (!SailIR.empty()) {
    SailCode = getSailSemantics(Inst->getName());
  }

  // If no Emulate field and no SAIL code, skip
  bool HasManualCode = !EmulateCode.empty() && !EmulateCode.trim().empty();
  bool HasSailCode = !SailCode.empty();

  if (!HasManualCode && !HasSailCode)
    return;

  StringRef Namespace = Inst->getValueAsString("Namespace");
  OS << "  case " << Namespace << "::" << Inst->getName() << ": {\n";

  // Check feature predicates if enabled
  if (EmitFeatureChecks) {
    std::vector<const Record *> Predicates =
        Inst->getValueAsListOfDefs("Predicates");
    for (const Record *Pred : Predicates) {
      if (!Pred->isValueUnset("PredicateName")) {
        StringRef FeatureName = Pred->getValueAsString("PredicateName");
        OS << "    if (!hasFeature(" << Namespace << "::" << FeatureName
           << ")) goto unhandled;\n";
      }
    }
  }

  // Manual Emulate field takes priority over SAIL-generated code
  if (HasManualCode) {
    // Find and emit variable dependencies
    StringSet<> Refs = findVariableRefs(EmulateCode);
    StringSet<> Emitted;
    for (const auto &Ref : Refs) {
      emitVariable(OS, Ref.getKey(), Inst, Emitted);
    }

    // Emit the emulate code
    std::string Code = substituteVars(EmulateCode.trim());
    SmallVector<StringRef, 16> Lines;
    StringRef(Code).split(Lines, '\n', -1, false);
    for (StringRef Line : Lines) {
      StringRef Trimmed = Line.trim();
      if (!Trimmed.empty())
        OS << "    " << Trimmed << "\n";
    }
  } else {
    // Use SAIL-generated code
    OS << "    // Generated from SAIL specification\n";
    OS << SailCode;
  }

  OS << "    break;\n";
  OS << "  }\n";
}

void EmulatorEmitter::run(raw_ostream &OS) {
  emitSourceFileHeader("Instruction Emulator", OS);

  // Collect Instruction records that have either:
  // 1. Non-empty Emulate field, OR
  // 2. SAIL semantics available
  std::vector<const Record *> RecordsToProcess;
  size_t ManualCount = 0;
  size_t SailCount = 0;

  ArrayRef<const Record *> Instructions =
      Records.getAllDerivedDefinitions("Instruction");
  for (const Record *Inst : Instructions) {
    bool HasEmulate = false;
    bool HasSail = false;

    // Check for manual Emulate field
    const RecordVal *EmuField = Inst->getValue("Emulate");
    if (EmuField) {
      StringRef EmulateCode = lookupVariable(Inst, "Emulate");
      if (!EmulateCode.empty() && !EmulateCode.trim().empty()) {
        HasEmulate = true;
        ManualCount++;
      }
    }

    // Check for SAIL semantics
    if (!SailIR.empty()) {
      std::string SailCode = getSailSemantics(Inst->getName());
      if (!SailCode.empty()) {
        HasSail = true;
        if (!HasEmulate)
          SailCount++;
      }
    }

    if (HasEmulate || HasSail) {
      RecordsToProcess.push_back(Inst);
    }
  }

  if (RecordsToProcess.empty()) {
    OS << "// No emulatable instructions found.\n";
    OS << "// Add 'let Emulate = [{ ... }]' to instruction definitions,\n";
    OS << "// or provide a SAIL IR file with -sail-ir=<path>.\n";
    return;
  }

  OS << "// Generated instruction emulation switch cases.\n";
  OS << "// Include this file inside a switch(Inst.getOpcode()) block.\n";
  OS << "// Instructions: " << Instructions.size() << " total, "
     << RecordsToProcess.size() << " emulatable";
  if (!SailIR.empty()) {
    OS << " (" << ManualCount << " manual, " << SailCount << " from SAIL)";
  }
  OS << "\n\n";

  OS << "#ifdef GET_EMULATOR_CASES\n";

  for (const Record *Rec : RecordsToProcess) {
    emitInstructionCase(OS, Rec);
  }

  OS << "#endif // GET_EMULATOR_CASES\n";
}

static TableGen::Emitter::OptClass<EmulatorEmitter>
    X("gen-emulator", "Generate instruction emulator");
