//===- EmulatorEmitter.cpp - Generate emulator from SAIL IR ---------------===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This TableGen backend generates C++ emulator code from SAIL Jib IR.
// It is completely target-agnostic.
//
// Architecture:
//   Source --> Lexer --> Parser --> AST --> CodeGen --> C++
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"

using namespace llvm;

static cl::opt<std::string> SailIRFile(
    "sail-ir", cl::desc("Path to SAIL Jib IR file"), cl::init(""));

namespace {

//===----------------------------------------------------------------------===//
// Part 1: Name handling
//===----------------------------------------------------------------------===//

/// Convert a SAIL z-encoded identifier to a valid C++ identifier.
/// SAIL encodes all identifiers with a 'z' prefix and special char sequences.
std::string toCppIdent(StringRef S) {
  // Handle $-prefixed temporaries (e.g., $0, $1077)
  if (!S.empty() && S[0] == '$')
    return ("tmp" + S.substr(1)).str();

  // Not z-encoded
  if (S.empty() || S[0] != 'z')
    return S.str();

  // First pass: decode z-sequences to get the original identifier
  std::string Decoded;
  Decoded.reserve(S.size());

  for (size_t I = 1; I < S.size(); ++I) {
    char C = S[I];
    if (C != 'z') {
      Decoded += C;
      continue;
    }
    // Decode z-sequence
    if (++I >= S.size())
      break;
    C = S[I];
    // Decode based on SAIL's zencode.rs
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

  // Second pass: convert to valid C++ identifier
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
        continue; // Skip, will be handled by >
      Result += '_';
    } else if (C == ' ' || C == '(' || C == ')') {
      // Skip whitespace and parens
    } else {
      Result += '_';
    }
  }

  // Clean up multiple underscores and leading/trailing underscores
  std::string Clean;
  for (char C : Result) {
    if (C == '_' && !Clean.empty() && Clean.back() == '_')
      continue;
    Clean += C;
  }
  while (!Clean.empty() && Clean.back() == '_')
    Clean.pop_back();
  while (!Clean.empty() && Clean.front() == '_')
    Clean.erase(0, 1);

  // If result starts with a digit, prefix with tmp
  if (!Clean.empty() && isdigit(Clean[0]))
    Clean = "tmp" + Clean;

  // Handle C++ reserved words
  if (Clean == "unsigned" || Clean == "signed" || Clean == "int" ||
      Clean == "bool" || Clean == "char" || Clean == "void" ||
      Clean == "return" || Clean == "if" || Clean == "else" ||
      Clean == "for" || Clean == "while" || Clean == "do" ||
      Clean == "switch" || Clean == "case" || Clean == "break" ||
      Clean == "continue" || Clean == "goto" || Clean == "default")
    Clean = "_" + Clean;

  return Clean;
}

//===----------------------------------------------------------------------===//
// Part 2: Tokens and Lexer
//===----------------------------------------------------------------------===//

enum class Tok {
  // Literals
  Id, Nat, Hex, Bin, String,
  // Keywords
  KwFn, KwVal, KwEnum, KwUnion, KwRegister, KwLet, KwJump, KwGoto, KwEnd,
  KwTrue, KwFalse, KwIs, KwAs, KwReturn,
  // Types (%-prefixed)
  TyI, TyI64, TyBv, TyUnit, TyBool, TyEnum, TyStruct,
  // Operators (@-prefixed)
  Op,
  // Punctuation
  LParen, RParen, LBrace, RBrace, Colon, Eq, Comma, Semi, Arrow, Dot,
  // Special
  Eof, Error
};

class Lexer {
  StringRef Input;
  size_t Pos = 0;
  Tok CurTok = Tok::Eof;
  std::string CurText;
  int64_t CurNum = 0;

public:
  explicit Lexer(StringRef S) : Input(S) { advance(); }

  Tok tok() const { return CurTok; }
  StringRef text() const { return CurText; }
  int64_t num() const { return CurNum; }

  Tok advance() {
    skipWhitespace();
    if (Pos >= Input.size())
      return CurTok = Tok::Eof;

    char C = Input[Pos];

    // Single-char tokens
    if (C == '(') { ++Pos; return CurTok = Tok::LParen; }
    if (C == ')') { ++Pos; return CurTok = Tok::RParen; }
    if (C == '{') { ++Pos; return CurTok = Tok::LBrace; }
    if (C == '}') { ++Pos; return CurTok = Tok::RBrace; }
    if (C == ':') { ++Pos; return CurTok = Tok::Colon; }
    if (C == ',') { ++Pos; return CurTok = Tok::Comma; }
    if (C == ';') { ++Pos; return CurTok = Tok::Semi; }
    if (C == '.') { ++Pos; return CurTok = Tok::Dot; }

    // Arrow ->
    if (C == '-' && Pos + 1 < Input.size() && Input[Pos + 1] == '>') {
      Pos += 2;
      return CurTok = Tok::Arrow;
    }

    // Equals
    if (C == '=') { ++Pos; return CurTok = Tok::Eq; }

    // String literal
    if (C == '"') {
      ++Pos;
      size_t Start = Pos;
      while (Pos < Input.size() && Input[Pos] != '"')
        ++Pos;
      CurText = Input.substr(Start, Pos - Start).str();
      if (Pos < Input.size())
        ++Pos;
      return CurTok = Tok::String;
    }

    // Hex literal 0x...
    if (C == '0' && Pos + 1 < Input.size() && Input[Pos + 1] == 'x') {
      size_t Start = Pos;
      Pos += 2;
      while (Pos < Input.size() && isxdigit(Input[Pos]))
        ++Pos;
      CurText = Input.substr(Start, Pos - Start).str();
      CurNum = std::strtoll(CurText.c_str() + 2, nullptr, 16);
      return CurTok = Tok::Hex;
    }

    // Binary literal 0b...
    if (C == '0' && Pos + 1 < Input.size() && Input[Pos + 1] == 'b') {
      size_t Start = Pos;
      Pos += 2;
      while (Pos < Input.size() && (Input[Pos] == '0' || Input[Pos] == '1'))
        ++Pos;
      CurText = Input.substr(Start, Pos - Start).str();
      CurNum = std::strtoll(CurText.c_str() + 2, nullptr, 2);
      return CurTok = Tok::Bin;
    }

    // Number (including negative)
    if (isdigit(C) || (C == '-' && Pos + 1 < Input.size() && isdigit(Input[Pos + 1]))) {
      size_t Start = Pos;
      if (C == '-')
        ++Pos;
      while (Pos < Input.size() && isdigit(Input[Pos]))
        ++Pos;
      CurText = Input.substr(Start, Pos - Start).str();
      CurNum = std::strtoll(CurText.c_str(), nullptr, 10);
      return CurTok = Tok::Nat;
    }

    // Type (%-prefixed)
    if (C == '%') {
      size_t Start = Pos;
      ++Pos;
      while (Pos < Input.size() && (isalnum(Input[Pos]) || Input[Pos] == '_'))
        ++Pos;
      CurText = Input.substr(Start, Pos - Start).str();
      StringRef T = CurText;
      if (T == "%i")
        return CurTok = Tok::TyI;
      if (T == "%i64")
        return CurTok = Tok::TyI64;
      if (T.starts_with("%bv"))
        return CurTok = Tok::TyBv;
      if (T == "%unit")
        return CurTok = Tok::TyUnit;
      if (T == "%bool")
        return CurTok = Tok::TyBool;
      if (T.starts_with("%enum"))
        return CurTok = Tok::TyEnum;
      if (T.starts_with("%struct") || T.starts_with("%union"))
        return CurTok = Tok::TyStruct;
      return CurTok = Tok::TyBv; // Default for unknown %types
    }

    // Operator (@-prefixed)
    if (C == '@') {
      size_t Start = Pos;
      ++Pos;
      // Handle turbofish: @op::<N>
      while (Pos < Input.size() && (isalnum(Input[Pos]) || Input[Pos] == '_' ||
                                     Input[Pos] == ':' || Input[Pos] == '<' ||
                                     Input[Pos] == '>'))
        ++Pos;
      CurText = Input.substr(Start, Pos - Start).str();
      return CurTok = Tok::Op;
    }

    // Backtick (source location) - skip to end of line
    if (C == '`') {
      while (Pos < Input.size() && Input[Pos] != '\n' && Input[Pos] != ';')
        ++Pos;
      return advance(); // Get next real token
    }

    // Identifier or keyword
    if (isalpha(C) || C == '_' || C == '$') {
      size_t Start = Pos;
      while (Pos < Input.size() && (isalnum(Input[Pos]) || Input[Pos] == '_' ||
                                     Input[Pos] == '$' || Input[Pos] == '\''))
        ++Pos;
      CurText = Input.substr(Start, Pos - Start).str();
      StringRef T = CurText;
      if (T == "fn")
        return CurTok = Tok::KwFn;
      if (T == "val")
        return CurTok = Tok::KwVal;
      if (T == "enum")
        return CurTok = Tok::KwEnum;
      if (T == "union")
        return CurTok = Tok::KwUnion;
      if (T == "register")
        return CurTok = Tok::KwRegister;
      if (T == "let")
        return CurTok = Tok::KwLet;
      if (T == "jump")
        return CurTok = Tok::KwJump;
      if (T == "goto")
        return CurTok = Tok::KwGoto;
      if (T == "end")
        return CurTok = Tok::KwEnd;
      if (T == "true")
        return CurTok = Tok::KwTrue;
      if (T == "false")
        return CurTok = Tok::KwFalse;
      if (T == "is")
        return CurTok = Tok::KwIs;
      if (T == "as")
        return CurTok = Tok::KwAs;
      if (T == "return")
        return CurTok = Tok::KwReturn;
      return CurTok = Tok::Id;
    }

    // Unknown - skip
    ++Pos;
    return advance();
  }

  bool at(Tok T) const { return CurTok == T; }
  bool atEnd() const { return CurTok == Tok::Eof; }

  bool consume(Tok T) {
    if (CurTok == T) {
      advance();
      return true;
    }
    return false;
  }

private:
  void skipWhitespace() {
    while (Pos < Input.size()) {
      char C = Input[Pos];
      if (C == ' ' || C == '\t' || C == '\n' || C == '\r') {
        ++Pos;
        continue;
      }
      // Skip // comments
      if (C == '/' && Pos + 1 < Input.size() && Input[Pos + 1] == '/') {
        while (Pos < Input.size() && Input[Pos] != '\n')
          ++Pos;
        continue;
      }
      break;
    }
  }
};

//===----------------------------------------------------------------------===//
// Part 3: AST
//===----------------------------------------------------------------------===//

struct Type {
  enum Kind { I, I64, Bv, Unit, Bool, Enum, Struct } K;
  int Width = 0;        // For Bv types: 8, 16, 32, 64, or 0 (unspecified)
  std::string Name;     // For Enum types

  std::string toCpp() const {
    switch (K) {
    case I:
    case I64:
      return "int64_t";
    case Bv:
      if (Width == 1) return "uint8_t";
      if (Width == 8) return "uint8_t";
      if (Width == 16) return "uint16_t";
      if (Width == 32) return "uint32_t";
      return "uint64_t";
    case Unit:
      return "void";
    case Bool:
      return "bool";
    case Enum:
      return "int";
    case Struct:
      if (Name.empty())
        return "int";
      return Name;
    }
    return "uint64_t";
  }
};

struct Exp {
  enum Kind {
    Ident, Nat, Hex, Bin, True, False, Unit,
    Op, Call, Field, Is, As
  } K;
  std::string Text;
  int64_t Num = 0;
  std::vector<Exp> Args;
  std::string OpName;   // For Op: the operator name (without @)
  int OpWidth = 0;      // For Op with turbofish: the width parameter
};

struct Instr {
  enum Kind { Decl, Init, Copy, Jump, Goto, End, Return } K;
  std::string Name;     // Variable name for Decl/Init/Copy
  Type Ty;              // Type for Decl/Init
  Exp Value;            // RHS for Init/Copy, condition for Jump
  int64_t Target = 0;   // Jump/Goto target
  int Line = 0;         // Source line number (for label generation)
};

struct FnDef {
  std::string Name;
  std::vector<std::string> Params;
  std::vector<Type> ParamTypes;
  Type ReturnType;
  std::vector<Instr> Body;
};

struct ValDecl {
  std::string Name;
  std::string ExternalName; // For external functions
  std::vector<Type> ParamTypes;
  Type ReturnType;
};

struct EnumDef {
  std::string Name;
  std::vector<std::string> Variants;
};

struct UnionDef {
  std::string Name;
  std::vector<std::pair<std::string, Type>> Variants;
};

struct RegisterDef {
  std::string Name;
  Type Ty;
};

struct LetDef {
  std::string Name;
  Type Ty;
  std::vector<Instr> Body;
};

struct JibIR {
  std::vector<RegisterDef> Registers;
  std::vector<EnumDef> Enums;
  std::vector<UnionDef> Unions;
  std::vector<ValDecl> Vals;
  std::vector<FnDef> Functions;
  std::vector<LetDef> Lets;
};

//===----------------------------------------------------------------------===//
// Part 4: Parser
//===----------------------------------------------------------------------===//

class Parser {
  Lexer &L;
  int LineNum = 0;

public:
  explicit Parser(Lexer &L) : L(L) {}

  JibIR parse() {
    JibIR IR;
    while (!L.atEnd()) {
      if (L.at(Tok::KwRegister)) {
        IR.Registers.push_back(parseRegister());
      } else if (L.at(Tok::KwEnum)) {
        IR.Enums.push_back(parseEnum());
      } else if (L.at(Tok::KwUnion)) {
        IR.Unions.push_back(parseUnion());
      } else if (L.at(Tok::KwVal)) {
        IR.Vals.push_back(parseVal());
      } else if (L.at(Tok::KwFn)) {
        IR.Functions.push_back(parseFn());
      } else if (L.at(Tok::KwLet)) {
        IR.Lets.push_back(parseLet());
      } else {
        L.advance();
      }
    }
    return IR;
  }

private:
  Type parseType() {
    Type T;
    switch (L.tok()) {
    case Tok::TyI:
      T.K = Type::I;
      break;
    case Tok::TyI64:
      T.K = Type::I64;
      break;
    case Tok::TyBv: {
      T.K = Type::Bv;
      // Extract width from %bvN
      StringRef S = L.text();
      if (S.size() > 3)
        S.substr(3).getAsInteger(10, T.Width);
      break;
    }
    case Tok::TyUnit:
      T.K = Type::Unit;
      break;
    case Tok::TyBool:
      T.K = Type::Bool;
      break;
    case Tok::TyEnum:
      T.K = Type::Enum;
      break;
    case Tok::TyStruct:
      T.K = Type::Struct;
      break;
    default:
      T.K = Type::Bv; // Default
    }
    L.advance();
    // Handle "%enum id" and "%struct id" forms - consume the following identifier
    if ((T.K == Type::Enum || T.K == Type::Struct) && L.at(Tok::Id)) {
      T.Name = L.text().str();
      L.advance();
    }
    return T;
  }

  Exp parseExp() {
    Exp E;

    // Literals
    if (L.at(Tok::KwTrue)) {
      E.K = Exp::True;
      L.advance();
    } else if (L.at(Tok::KwFalse)) {
      E.K = Exp::False;
      L.advance();
    } else if (L.at(Tok::Nat)) {
      E.K = Exp::Nat;
      E.Num = L.num();
      E.Text = L.text().str();
      L.advance();
    } else if (L.at(Tok::Hex)) {
      E.K = Exp::Hex;
      E.Num = L.num();
      E.Text = L.text().str();
      L.advance();
    } else if (L.at(Tok::Bin)) {
      E.K = Exp::Bin;
      E.Num = L.num();
      E.Text = L.text().str();
      L.advance();
    } else if (L.at(Tok::LParen) && peekUnit()) {
      // Unit literal ()
      E.K = Exp::Unit;
      L.advance(); // (
      L.advance(); // )
    } else if (L.at(Tok::Op)) {
      // Operator: @op(args) or @op::<N>(args)
      E.K = Exp::Op;
      StringRef OpText = L.text();
      // Parse @op or @op::<N>
      size_t ColonPos = OpText.find("::<");
      if (ColonPos != StringRef::npos) {
        E.OpName = OpText.substr(1, ColonPos - 1).str();
        size_t EndPos = OpText.find('>');
        if (EndPos != StringRef::npos) {
          StringRef WidthStr = OpText.substr(ColonPos + 3, EndPos - ColonPos - 3);
          WidthStr.getAsInteger(10, E.OpWidth);
        }
      } else {
        E.OpName = OpText.substr(1).str();
      }
      L.advance();
      E.Args = parseArgList();
    } else if (L.at(Tok::Id)) {
      E.K = Exp::Ident;
      E.Text = L.text().str();
      L.advance();
      // Check for function call: id(args)
      if (L.at(Tok::LParen)) {
        E.K = Exp::Call;
        E.Args = parseArgList();
      }
    } else {
      // Unknown - return empty ident
      E.K = Exp::Ident;
      E.Text = "";
    }

    // Postfix: .field, is id, as id
    while (true) {
      if (L.at(Tok::Dot)) {
        L.advance();
        Exp Field;
        Field.K = Exp::Field;
        Field.Args.push_back(std::move(E));
        Field.Text = L.text().str();
        L.advance();
        E = std::move(Field);
      } else if (L.at(Tok::KwIs)) {
        L.advance();
        Exp Is;
        Is.K = Exp::Is;
        Is.Args.push_back(std::move(E));
        Is.Text = L.text().str();
        L.advance();
        E = std::move(Is);
      } else if (L.at(Tok::KwAs)) {
        L.advance();
        Exp As;
        As.K = Exp::As;
        As.Args.push_back(std::move(E));
        As.Text = L.text().str();
        L.advance();
        E = std::move(As);
      } else {
        break;
      }
    }

    return E;
  }

  std::vector<Exp> parseArgList() {
    std::vector<Exp> Args;
    if (!L.consume(Tok::LParen))
      return Args;
    if (!L.at(Tok::RParen)) {
      Args.push_back(parseExp());
      while (L.consume(Tok::Comma))
        Args.push_back(parseExp());
    }
    L.consume(Tok::RParen);
    return Args;
  }

  bool peekUnit() {
    // Save position and check for ()
    // This is a simple lookahead for unit literal
    return L.at(Tok::LParen);
  }

  Instr parseInstr() {
    Instr I;
    I.Line = LineNum++;

    if (L.at(Tok::KwJump)) {
      I.K = Instr::Jump;
      L.advance();
      I.Value = parseExp();
      L.consume(Tok::KwGoto);
      if (L.at(Tok::Nat)) {
        I.Target = L.num();
        L.advance();
      }
    } else if (L.at(Tok::KwGoto)) {
      I.K = Instr::Goto;
      L.advance();
      if (L.at(Tok::Nat)) {
        I.Target = L.num();
        L.advance();
      }
    } else if (L.at(Tok::KwEnd)) {
      I.K = Instr::End;
      L.advance();
    } else if (L.at(Tok::KwReturn)) {
      I.K = Instr::Return;
      L.advance();
      if (L.consume(Tok::Eq))
        I.Value = parseExp();
    } else if (L.at(Tok::Id)) {
      std::string Name = L.text().str();
      L.advance();

      if (L.at(Tok::Colon)) {
        // Decl or Init: id : Ty [= Exp]
        L.advance();
        Type Ty = parseType();
        if (L.consume(Tok::Eq)) {
          I.K = Instr::Init;
          I.Name = Name;
          I.Ty = Ty;
          I.Value = parseExp();
        } else {
          I.K = Instr::Decl;
          I.Name = Name;
          I.Ty = Ty;
        }
      } else if (L.consume(Tok::Eq)) {
        // Copy: id = Exp
        I.K = Instr::Copy;
        I.Name = Name;
        I.Value = parseExp();
      }
    }

    L.consume(Tok::Semi);
    return I;
  }

  RegisterDef parseRegister() {
    RegisterDef R;
    L.advance(); // register
    R.Name = L.text().str();
    L.advance();
    L.consume(Tok::Colon);
    R.Ty = parseType();
    return R;
  }

  EnumDef parseEnum() {
    EnumDef E;
    L.advance(); // enum
    E.Name = L.text().str();
    L.advance();
    L.consume(Tok::LBrace);
    while (!L.at(Tok::RBrace) && !L.atEnd()) {
      if (L.at(Tok::Id)) {
        E.Variants.push_back(L.text().str());
        L.advance();
      }
      L.consume(Tok::Comma);
    }
    L.consume(Tok::RBrace);
    return E;
  }

  UnionDef parseUnion() {
    UnionDef U;
    L.advance(); // union
    U.Name = L.text().str();
    L.advance();
    L.consume(Tok::LBrace);
    while (!L.at(Tok::RBrace) && !L.atEnd()) {
      if (L.at(Tok::Id)) {
        std::string Name = L.text().str();
        L.advance();
        if (!L.consume(Tok::Colon)) {
          // Not a valid variant - skip this token and continue
          continue;
        }
        Type Ty = parseType();
        U.Variants.emplace_back(Name, Ty);
      } else {
        // Skip unexpected tokens to guarantee progress
        L.advance();
      }
      L.consume(Tok::Comma);
    }
    L.consume(Tok::RBrace);
    return U;
  }

  ValDecl parseVal() {
    ValDecl V;
    L.advance(); // val
    V.Name = L.text().str();
    L.advance();

    // Check for external: val id = "name" : ...
    if (L.consume(Tok::Eq)) {
      if (L.at(Tok::String)) {
        V.ExternalName = L.text().str();
        L.advance();
      }
    }

    L.consume(Tok::Colon);

    // Parse (params) -> return
    if (L.consume(Tok::LParen)) {
      while (!L.at(Tok::RParen) && !L.atEnd()) {
        V.ParamTypes.push_back(parseType());
        L.consume(Tok::Comma);
      }
      L.consume(Tok::RParen);
    }
    L.consume(Tok::Arrow);
    V.ReturnType = parseType();

    return V;
  }

  FnDef parseFn() {
    FnDef F;
    L.advance(); // fn
    F.Name = L.text().str();
    L.advance();

    L.consume(Tok::LParen);
    while (!L.at(Tok::RParen) && !L.atEnd()) {
      if (L.at(Tok::Id)) {
        F.Params.push_back(L.text().str());
        L.advance();
      }
      L.consume(Tok::Comma);
    }
    L.consume(Tok::RParen);

    // Parse body
    L.consume(Tok::LBrace);
    LineNum = 0;
    while (!L.at(Tok::RBrace) && !L.atEnd()) {
      F.Body.push_back(parseInstr());
    }
    L.consume(Tok::RBrace);

    return F;
  }

  LetDef parseLet() {
    LetDef Let;
    L.advance(); // let
    L.consume(Tok::LParen);
    Let.Name = L.text().str();
    L.advance();
    L.consume(Tok::Colon);
    Let.Ty = parseType();
    L.consume(Tok::RParen);
    L.consume(Tok::LBrace);
    LineNum = 0;
    while (!L.at(Tok::RBrace) && !L.atEnd()) {
      Let.Body.push_back(parseInstr());
    }
    L.consume(Tok::RBrace);
    return Let;
  }
};

//===----------------------------------------------------------------------===//
// Part 5: Code Generator
//===----------------------------------------------------------------------===//

class CodeGen {
  const JibIR &IR;
  raw_ostream &OS;

  // Maps for lookups
  StringMap<const ValDecl *> ValsByName;
  StringMap<const UnionDef *> UnionsByName;
  StringSet<> UnionVariants;

public:
  CodeGen(const JibIR &IR, raw_ostream &OS) : IR(IR), OS(OS) {
    for (const auto &V : IR.Vals)
      ValsByName[V.Name] = &V;
    for (const auto &U : IR.Unions) {
      UnionsByName[U.Name] = &U;
      for (const auto &V : U.Variants)
        UnionVariants.insert(V.first);
    }
  }

  void emit() {
    const UnionDef *InstrUnion = nullptr;
    const FnDef *ExecuteFn = nullptr;

    for (const auto &U : IR.Unions)
      if (U.Name == "zinstruction")
        InstrUnion = &U;
    for (const auto &F : IR.Functions)
      if (F.Name == "zexecute")
        ExecuteFn = &F;

    if (!InstrUnion || !ExecuteFn) {
      OS << "// No instruction union or execute function found\n";
      return;
    }

    // Count instructions
    size_t NumInstr = InstrUnion->Variants.size();
    OS << "// Generated from SAIL Jib IR\n";
    OS << "// Instructions: " << NumInstr << " from SAIL\n\n";

    // Emit instruction cases
    OS << "#ifdef GET_EMULATOR_CASES\n";
    emitInstructionCases(*ExecuteFn, *InstrUnion);
    OS << "#endif // GET_EMULATOR_CASES\n\n";

    // Emit types (struct wrappers and variant typedef) - used at namespace scope
    OS << "#ifdef GET_EMULATOR_TYPES\n";
    emitTypes(*InstrUnion);
    OS << "#endif // GET_EMULATOR_TYPES\n\n";

    // Emit enums
    OS << "#ifdef GET_EMULATOR_ENUMS\n";
    emitEnums();
    OS << "#endif // GET_EMULATOR_ENUMS\n\n";

    // Emit member variable declarations (let bindings) - used inside class
    OS << "#ifdef GET_EMULATOR_MEMBERS\n";
    emitMembers();
    OS << "#endif // GET_EMULATOR_MEMBERS\n\n";

    // Emit helper methods (as class members)
    OS << "#ifdef GET_EMULATOR_METHODS\n";
    emitMethods();
    OS << "#endif // GET_EMULATOR_METHODS\n\n";

    // Also emit as standalone functions for compatibility
    OS << "#ifdef GET_EMULATOR_FUNCTIONS\n";
    emitFunctions();
    OS << "#endif // GET_EMULATOR_FUNCTIONS\n\n";

    // Emit MCInst to SAIL instruction mapping function
    OS << "#ifdef GET_EMULATOR_MAPPING\n";
    emitMcInstToSailMapping(*InstrUnion);
    OS << "#endif // GET_EMULATOR_MAPPING\n";
  }

private:
  void emitInstructionCases(const FnDef &Execute, const UnionDef &InstrUnion) {
    // The execute function has a pattern:
    //   jump arg is INSTR goto N
    //   ... instruction body ...
    //   goto END

    std::string ArgName = Execute.Params.empty() ? "arg" : Execute.Params[0];

    // Extract instruction bodies by parsing the execute function
    struct InstrBody {
      std::string Name;
      std::vector<Instr> Body;
    };
    std::vector<InstrBody> Bodies;

    InstrBody *Current = nullptr;
    for (const auto &I : Execute.Body) {
      if (I.K == Instr::Jump && I.Value.K == Exp::Is) {
        // Start of new instruction
        Bodies.emplace_back();
        Current = &Bodies.back();
        Current->Name = toCppIdent(I.Value.Text);
      } else if (I.K == Instr::Goto) {
        // End of instruction - skip
        Current = nullptr;
      } else if (Current) {
        Current->Body.push_back(I);
      }
    }

    // Emit each instruction
    for (const auto &B : Bodies) {
      OS << "case " << B.Name << ": {\n";
      emitInstrBody(B.Body, ArgName, B.Name);
      OS << "  break;\n}\n";
    }
  }

  void emitInstrBody(const std::vector<Instr> &Body, StringRef ArgName,
                     StringRef InstrName, StringRef ReturnVar = "") {
    // Collect jump targets for label emission
    DenseSet<int64_t> JumpTargets;
    for (const auto &I : Body) {
      if (I.K == Instr::Jump || I.K == Instr::Goto)
        JumpTargets.insert(I.Target);
    }

    // Track unit-type variables
    StringSet<> UnitVars;
    // Track declared variables (to avoid re-declaring in Init)
    StringSet<> DeclaredVars;

    // First pass: emit all declarations at the top (avoids goto issues)
    for (const auto &I : Body) {
      if (I.K != Instr::Decl && I.K != Instr::Init)
        continue;
      std::string Name = I.Name;
      if (!Name.empty() && Name[0] == '$')
        Name = "tmp" + Name.substr(1);
      if (I.Ty.K == Type::Unit) {
        UnitVars.insert(Name);
      } else if (!DeclaredVars.contains(Name)) {
        OS << "  " << I.Ty.toCpp() << " " << Name << ";\n";
        DeclaredVars.insert(Name);
      }
    }

    // Second pass: emit code
    for (const auto &I : Body) {
      if (JumpTargets.contains(I.Line))
        OS << "L" << I.Line << ":;\n";

      switch (I.K) {
      case Instr::Decl:
        // Already emitted in first pass
        break;

      case Instr::Init: {
        std::string Name = I.Name;
        if (!Name.empty() && Name[0] == '$')
          Name = "tmp" + Name.substr(1);
        std::string Val = emitExp(I.Value, ArgName, InstrName);
        if (I.Ty.K == Type::Unit) {
          if (!Val.empty() && Val != "{}")
            OS << "  " << Val << ";\n";
        } else if (!Val.empty()) {
          OS << "  " << Name << " = " << Val << ";\n";
        }
        break;
      }

      case Instr::Copy: {
        std::string Name = I.Name;
        if (!Name.empty() && Name[0] == '$')
          Name = "tmp" + Name.substr(1);
        std::string Val = emitExp(I.Value, ArgName, InstrName);
        bool IsInstrBody = !InstrName.empty();
        // Skip if value is a unit-type variable (would be undeclared)
        if (UnitVars.contains(Val))
          break;
        if (IsInstrBody && (Name == "tmp0" || Name == "return")) {
          if (!Val.empty() && Val != "{}")
            OS << "  " << Val << ";\n";
        } else if (!ReturnVar.empty() && Name == ReturnVar) {
          if (!Val.empty() && Val != "{}")
            OS << "  return " << Val << ";\n";
        } else if (UnitVars.contains(Name)) {
          if (!Val.empty() && Val != "{}")
            OS << "  " << Val << ";\n";
        } else if (!Val.empty()) {
          OS << "  " << Name << " = " << Val << ";\n";
        }
        break;
      }

      case Instr::Jump:
        OS << "  if (" << emitExp(I.Value, ArgName, InstrName)
           << ") goto L" << I.Target << ";\n";
        break;

      case Instr::Goto:
        OS << "  goto L" << I.Target << ";\n";
        break;

      case Instr::Return: {
        std::string Val = emitExp(I.Value, ArgName, InstrName);
        if (Val.empty() || Val == "{}")
          OS << "  return;\n";
        else
          OS << "  return " << Val << ";\n";
        break;
      }

      case Instr::End:
        break;
      }
    }
  }

  std::string emitExp(const Exp &E, StringRef ArgName, StringRef InstrName) {
    switch (E.K) {
    case Exp::Ident:
      if (!E.Text.empty() && E.Text[0] == '$')
        return "tmp" + E.Text.substr(1);
      return E.Text;

    case Exp::Nat:
    case Exp::Hex:
    case Exp::Bin:
      return E.Text;

    case Exp::True:
      return "true";

    case Exp::False:
      return "false";

    case Exp::Unit:
      return "{}";

    case Exp::Op:
      return emitOp(E, ArgName, InstrName);

    case Exp::Call:
      return emitCall(E, ArgName, InstrName);

    case Exp::Field:
      return emitExp(E.Args[0], ArgName, InstrName) + "." + E.Text;

    case Exp::Is:
      // SAIL's "is" (Kind check) returns TRUE when the constructor does NOT match.
      // See isla-lib/src/executor.rs: Val::Ctor(ctor_b, _) => Val::Bool(*ctor_a != *ctor_b)
      return "!std::holds_alternative<" + E.Text + ">(" +
             emitExp(E.Args[0], ArgName, InstrName) + ")";

    case Exp::As:
      return "std::get<" + E.Text + ">(" +
             emitExp(E.Args[0], ArgName, InstrName) + ").value";
    }
    return "";
  }

  std::string emitOp(const Exp &E, StringRef ArgName, StringRef InstrName) {
    StringRef Op = E.OpName;
    auto &A = E.Args;

    // Binary operations
    if (A.size() == 2) {
      std::string L = emitExp(A[0], ArgName, InstrName);
      std::string R = emitExp(A[1], ArgName, InstrName);
      if (Op == "bvadd") return "(" + L + " + " + R + ")";
      if (Op == "bvsub") return "(" + L + " - " + R + ")";
      if (Op == "bvand") return "(" + L + " & " + R + ")";
      if (Op == "bvor")  return "(" + L + " | " + R + ")";
      if (Op == "bvxor") return "(" + L + " ^ " + R + ")";
      if (Op == "and")   return "(" + L + " && " + R + ")";
      if (Op == "or")    return "(" + L + " || " + R + ")";
      if (Op == "eq")    return "(" + L + " == " + R + ")";
      if (Op == "neq")   return "(" + L + " != " + R + ")";
      if (Op == "lt")    return "(" + L + " < " + R + ")";
      if (Op == "lteq")  return "(" + L + " <= " + R + ")";
      if (Op == "gt")    return "(" + L + " > " + R + ")";
      if (Op == "gteq")  return "(" + L + " >= " + R + ")";
      if (Op == "concat")
        return "((" + L + " << 8) | " + R + ")";
      if (Op == "slice" && E.OpWidth > 0)
        return "((" + L + " >> " + R + ") & ((1 << " +
               std::to_string(E.OpWidth) + ") - 1))";
    }

    // Unary operations
    if (A.size() == 1) {
      std::string X = emitExp(A[0], ArgName, InstrName);
      if (Op == "bvnot") return "(~" + X + ")";
      if (Op == "not")   return "(!" + X + ")";
      if (Op == "zero_extend") return X;
      if (Op == "signed" && E.OpWidth > 0)
        return "(int" + std::to_string(E.OpWidth) + "_t)" + X;
      if (Op == "unsigned" && E.OpWidth > 0)
        return "(uint" + std::to_string(E.OpWidth) + "_t)" + X;
    }

    // Unknown - emit as comment
    return "/* @" + E.OpName + " */";
  }

  std::string emitCall(const Exp &E, StringRef ArgName, StringRef InstrName) {
    std::string FnName = E.Text;

    // Build argument list
    std::string Args;
    for (size_t I = 0; I < E.Args.size(); ++I) {
      if (I > 0)
        Args += ", ";
      std::string Arg = emitExp(E.Args[I], ArgName, InstrName);
      if (Arg != "{}")
        Args += Arg;
    }

    // Union variants use brace init, functions use parens
    if (UnionVariants.contains(FnName))
      return FnName + "{" + Args + "}";
    return FnName + "(" + Args + ")";
  }

  void emitVariantStruct(StringRef Name, const Type &T) {
    if (T.K == Type::Unit) {
      OS << "struct " << Name << " {};\n";
    } else {
      OS << "struct " << Name << " { " << T.toCpp() << " value; };\n";
    }
  }

  void emitTypes(const UnionDef &InstrUnion) {
    OS << "// Instruction variant structs\n";
    for (const auto &V : InstrUnion.Variants)
      emitVariantStruct(V.first, V.second);
    OS << "\n";

    OS << "// Instruction union type\n";
    OS << "using " << InstrUnion.Name << " = std::variant<";
    bool First = true;
    for (const auto &V : InstrUnion.Variants) {
      if (!First)
        OS << ", ";
      First = false;
      OS << V.first;
    }
    OS << ">;\n\n";

    for (const auto &U : IR.Unions) {
      if (U.Name == InstrUnion.Name)
        continue;
      OS << "// " << U.Name << "\n";
      for (const auto &V : U.Variants)
        emitVariantStruct(V.first, V.second);
      OS << "using " << U.Name << " = std::variant<";
      First = true;
      for (const auto &V : U.Variants) {
        if (!First)
          OS << ", ";
        First = false;
        OS << V.first;
      }
      OS << ">;\n\n";
    }

    // Emit enums at namespace scope too
    for (const auto &E : IR.Enums) {
      OS << "// " << E.Name << "\n";
      for (size_t I = 0; I < E.Variants.size(); ++I)
        OS << "static constexpr int " << E.Variants[I] << " = " << I << ";\n";
      OS << "\n";
    }
    // Note: Let bindings are emitted in GET_EMULATOR_MEMBERS as class members
  }

  void emitMembers() {
    // Emit let member variable declarations (as class members, not namespace scope)
    if (!IR.Lets.empty()) {
      OS << "// Let bindings (initialized by initLets())\n";
      for (const auto &Let : IR.Lets)
        OS << Let.Ty.toCpp() << " " << Let.Name << ";\n";
      OS << "\n";
    }
  }

  void emitEnums() {
    for (const auto &E : IR.Enums) {
      OS << "// " << E.Name << "\n";
      for (size_t I = 0; I < E.Variants.size(); ++I)
        OS << "static constexpr int " << E.Variants[I] << " = " << I << ";\n";
      OS << "\n";
    }
  }

  void emitFunctions() {
    OS << "// Helper functions from SAIL\n\n";

    // Track which functions have IR bodies
    StringSet<> HasBody;
    for (const auto &F : IR.Functions)
      HasBody.insert(F.Name);

    // First emit external primitives (val declarations without fn bodies)
    for (const auto &V : IR.Vals) {
      if (V.ExternalName.empty())
        continue;
      if (HasBody.contains(V.Name))
        continue; // Has IR body, will be emitted below

      std::string Ret = V.ReturnType.toCpp();
      std::string Params;
      for (size_t I = 0; I < V.ParamTypes.size(); ++I) {
        if (I > 0)
          Params += ", ";
        Params += V.ParamTypes[I].toCpp() + " p" + std::to_string(I);
      }

      // Generate body based on external name
      StringRef Ext = V.ExternalName;
      std::string Body = getPrimitiveBody(Ext, V.ParamTypes.size());
      if (Body.empty())
        continue; // Unknown primitive - skip

      OS << Ret << " " << V.Name << "(" << Params << ") { " << Body << " }\n";
    }
    OS << "\n";

    // Then emit functions with IR bodies
    for (const auto &F : IR.Functions) {

      // Find return type from val declarations
      Type RetType;
      RetType.K = Type::Unit;
      std::vector<Type> ParamTypes;
      if (auto *V = ValsByName.lookup(F.Name)) {
        RetType = V->ReturnType;
        ParamTypes = V->ParamTypes;
      }

      // Emit function signature
      OS << RetType.toCpp() << " " << F.Name << "(";
      bool First = true;
      for (size_t I = 0; I < F.Params.size(); ++I) {
        Type PT = I < ParamTypes.size() ? ParamTypes[I] : Type{Type::Bv, 64, ""};
        // Skip void/unit parameters
        if (PT.K == Type::Unit)
          continue;
        if (!First)
          OS << ", ";
        First = false;
        OS << PT.toCpp() << " " << F.Params[I];
      }
      OS << ") {\n";

      // Emit body
      emitInstrBody(F.Body, "", "");

      OS << "}\n\n";
    }
  }

  void emitMethods() {
    // Emit functions as class methods (without static, called on 'this')
    OS << "// Helper methods from SAIL (as class members)\n\n";

    // Track which functions have IR bodies
    StringSet<> HasBody;
    for (const auto &F : IR.Functions)
      HasBody.insert(F.Name);

    // First emit external primitives
    for (const auto &V : IR.Vals) {
      if (V.ExternalName.empty())
        continue;
      if (HasBody.contains(V.Name))
        continue;

      std::string Ret = V.ReturnType.toCpp();
      std::string Params;
      for (size_t I = 0; I < V.ParamTypes.size(); ++I) {
        if (I > 0)
          Params += ", ";
        Params += V.ParamTypes[I].toCpp() + " p" + std::to_string(I);
      }

      StringRef Ext = V.ExternalName;
      std::string Body = getPrimitiveBody(Ext, V.ParamTypes.size());
      if (Body.empty())
        continue;

      OS << Ret << " " << V.Name << "(" << Params << ") { " << Body << " }\n";
    }
    OS << "\n";

    // Then emit functions with IR bodies
    for (const auto &F : IR.Functions) {
      // Find return type from val declarations
      Type RetType;
      RetType.K = Type::Unit;
      std::vector<Type> ParamTypes;
      if (auto *V = ValsByName.lookup(F.Name)) {
        RetType = V->ReturnType;
        ParamTypes = V->ParamTypes;
      }

      // Emit function signature
      OS << RetType.toCpp() << " " << F.Name << "(";
      bool First = true;
      for (size_t I = 0; I < F.Params.size(); ++I) {
        Type PT = I < ParamTypes.size() ? ParamTypes[I] : Type{Type::Bv, 64, ""};
        if (PT.K == Type::Unit)
          continue;
        if (!First)
          OS << ", ";
        First = false;
        OS << PT.toCpp() << " " << F.Params[I];
      }
      OS << ") {\n";

      // Emit body
      emitInstrBody(F.Body, "", "");

      OS << "}\n\n";
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

  void emitMcInstToSailMapping(const UnionDef &InstrUnion) {
    OS << "// Map MCInst opcode to SAIL instruction variant\n";
    OS << InstrUnion.Name << " mcInstToSail(const MCInst &Inst) {\n";
    OS << "  switch (Inst.getOpcode()) {\n";

    for (const auto &V : InstrUnion.Variants) {
      std::string OpcName = toCppIdent(V.first);
      OS << "  case " << OpcName << ":\n";
      if (V.second.K == Type::Unit) {
        OS << "    return " << V.first << "{};\n";
      } else {
        std::string Cast = getCastForType(V.second);
        OS << "    return " << V.first << "{" << Cast
           << "(Inst.getOperand(0).getImm())};\n";
      }
    }

    OS << "  default:\n";
    OS << "    llvm_unreachable(\"Unknown opcode in mcInstToSail\");\n";
    OS << "  }\n";
    OS << "}\n";
  }

  /// Get the appropriate cast for a SAIL type when extracting MCInst operands.
  std::string getCastForType(const Type &T) {
    switch (T.K) {
    case Type::Bv:
      if (T.Width == 8) return "static_cast<uint8_t>";
      if (T.Width == 16) return "static_cast<uint16_t>";
      if (T.Width == 32) return "static_cast<uint32_t>";
      return "static_cast<uint64_t>";
    case Type::Bool:
      return "static_cast<bool>";
    case Type::I:
    case Type::I64:
      return "static_cast<int64_t>";
    default:
      return "static_cast<uint64_t>";
    }
  }

  /// Get C++ implementation for a SAIL primitive based on its external name.
  std::string getPrimitiveBody(StringRef Ext, size_t NumParams) {
    // Arithmetic
    if (Ext == "add_bits" || Ext == "add_bits_int")
      return "return p0 + p1;";
    if (Ext == "sub_bits" || Ext == "sub_bits_int")
      return "return p0 - p1;";

    // Bitwise
    if (Ext == "and_bits")
      return "return p0 & p1;";
    if (Ext == "or_bits")
      return "return p0 | p1;";
    if (Ext == "xor_bits")
      return "return p0 ^ p1;";
    if (Ext == "not_bits")
      return "return ~p0;";

    // Shifts
    if (Ext == "shiftl")
      return "return p0 << p1;";
    if (Ext == "shiftr")
      return "return p0 >> p1;";

    // Concatenation
    if (Ext == "append")
      return "return (p0 << 8) | p1;";
    if (Ext == "append_64")
      return "(void)p0; return p1;";

    // Comparison
    if (Ext == "eq_bits")
      return "return p0 == p1;";
    if (Ext == "neq_bits")
      return "return p0 != p1;";
    if (Ext == "gteq")
      return "return p0 >= p1;";
    if (Ext == "lteq")
      return "return p0 <= p1;";
    if (Ext == "gt_int" || Ext == "gt")
      return "return p0 > p1;";
    if (Ext == "lt_int" || Ext == "lt")
      return "return p0 < p1;";

    // Type conversions - includes SAIL's %type->%type patterns
    if (Ext == "sail_unsigned" || Ext == "unsigned")
      return "return p0;";
    if (Ext == "sail_signed" || Ext == "signed")
      return "return (int64_t)p0;";
    if (Ext == "sign_extend")
      return "return (int64_t)(int8_t)p0;";
    if (Ext.contains("->"))
      return "return p0;";

    // Bit extraction
    if (Ext == "vector_subrange")
      return "return (p0 >> p2) & ((1ULL << (p1 - p2 + 1)) - 1);";

    // Boolean
    if (Ext == "and_bool")
      return "return p0 && p1;";
    if (Ext == "or_bool")
      return "return p0 || p1;";

    // Misc
    if (Ext == "undefined_bitvector")
      return "return 0;";

    return ""; // Unknown
  }
};

//===----------------------------------------------------------------------===//
// Part 6: EmulatorEmitter Entry Point
//===----------------------------------------------------------------------===//

class EmulatorEmitter {
public:
  EmulatorEmitter(const RecordKeeper &) {}

  void run(raw_ostream &OS) {
    emitSourceFileHeader("Instruction Emulator", OS);

    if (SailIRFile.empty()) {
      OS << "// No SAIL IR file provided (-sail-ir=<path>)\n";
      return;
    }

    // Read IR file
    ErrorOr<std::unique_ptr<MemoryBuffer>> BufOrErr =
        MemoryBuffer::getFile(SailIRFile);
    if (!BufOrErr) {
      PrintError("Failed to open SAIL IR file: " + SailIRFile);
      return;
    }

    // Parse
    Lexer L((*BufOrErr)->getBuffer());
    Parser P(L);
    JibIR IR = P.parse();

    // Generate
    CodeGen CG(IR, OS);
    CG.emit();
  }
};

} // end anonymous namespace

static TableGen::Emitter::OptClass<EmulatorEmitter>
    X("gen-emulator", "Generate instruction emulator from SAIL IR");
