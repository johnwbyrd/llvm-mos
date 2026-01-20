//===- EmulatorParser.h - Parser for SAIL Jib IR ---------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Parser for SAIL Jib IR. The parser is a simple
// recursive descent parser that produces an AST (JibIR) from lexer tokens.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_UTILS_TABLEGEN_EMULATORPARSER_H
#define LLVM_UTILS_TABLEGEN_EMULATORPARSER_H

#include "EmulatorAST.h"
#include "EmulatorLexer.h"

namespace llvm {
namespace emu {

/// Recursive descent parser for SAIL Jib IR.
class Parser {
  Lexer &Lex;
  int CurrentLineNumber = 0;
  bool DebugEnabled = false;

public:
  explicit Parser(Lexer &L) : Lex(L) {}

  /// Enable debug output for parser.
  void setDebug(bool Enable) { DebugEnabled = Enable; }

  /// Parse the complete IR and return the AST.
  JibIR parse() {
    JibIR IR;
    uint64_t LoopCount = 0;
    while (!Lex.atEnd()) {
      ++LoopCount;
      if (DebugEnabled && (LoopCount % 10000 == 0)) {
        llvm::errs() << "DEBUG Parser: loop " << LoopCount
                     << ", pos " << Lex.getPosition()
                     << ", token " << Lexer::tokenName(Lex.token())
                     << " '" << Lex.text() << "'\n";
      }
      if (Lex.at(Token::KwRegister)) {
        IR.Registers.push_back(parseRegister());
        if (DebugEnabled)
          llvm::errs() << "DEBUG Parser: parsed register, total="
                       << IR.Registers.size() << "\n";
      } else if (Lex.at(Token::KwEnum)) {
        IR.Enums.push_back(parseEnum());
        if (DebugEnabled)
          llvm::errs() << "DEBUG Parser: parsed enum, total="
                       << IR.Enums.size() << "\n";
      } else if (Lex.at(Token::KwUnion)) {
        IR.Unions.push_back(parseUnion());
        if (DebugEnabled)
          llvm::errs() << "DEBUG Parser: parsed union '"
                       << IR.Unions.back().Name << "', total="
                       << IR.Unions.size() << "\n";
      } else if (Lex.at(Token::KwVal)) {
        IR.Vals.push_back(parseVal());
        if (DebugEnabled)
          llvm::errs() << "DEBUG Parser: parsed val '"
                       << IR.Vals.back().Name << "', total="
                       << IR.Vals.size() << "\n";
      } else if (Lex.at(Token::KwFn)) {
        IR.Functions.push_back(parseFunction());
        if (DebugEnabled)
          llvm::errs() << "DEBUG Parser: parsed fn '"
                       << IR.Functions.back().Name << "', total="
                       << IR.Functions.size() << "\n";
      } else if (Lex.at(Token::KwLet)) {
        IR.Lets.push_back(parseLet());
        if (DebugEnabled)
          llvm::errs() << "DEBUG Parser: parsed let, total="
                       << IR.Lets.size() << "\n";
      } else {
        Lex.advance();
      }
    }
    if (DebugEnabled)
      llvm::errs() << "DEBUG Parser: done, total loops=" << LoopCount << "\n";
    return IR;
  }

private:
  //===--------------------------------------------------------------------===//
  // Type Parsing
  //===--------------------------------------------------------------------===//

  Type parseType() {
    Type Result;
    switch (Lex.token()) {
    case Token::TyI:
      Result.Kind = Type::TK_I;
      break;
    case Token::TyI64:
      Result.Kind = Type::TK_I64;
      break;
    case Token::TyBv: {
      Result.Kind = Type::TK_Bv;
      // Extract width from %bvN
      StringRef TypeText = Lex.text();
      if (TypeText.size() > 3)
        TypeText.substr(3).getAsInteger(10, Result.Width);
      break;
    }
    case Token::TyUnit:
      Result.Kind = Type::TK_Unit;
      break;
    case Token::TyBool:
      Result.Kind = Type::TK_Bool;
      break;
    case Token::TyEnum:
      Result.Kind = Type::TK_Enum;
      break;
    case Token::TyStruct:
      Result.Kind = Type::TK_Struct;
      break;
    default:
      Result.Kind = Type::TK_Bv;
    }
    Lex.advance();

    // Handle "%enum id" and "%struct id" forms
    if ((Result.Kind == Type::TK_Enum || Result.Kind == Type::TK_Struct) &&
        Lex.at(Token::Ident)) {
      Result.Name = Lex.text().str();
      Lex.advance();
    }
    return Result;
  }

  //===--------------------------------------------------------------------===//
  // Expression Parsing
  //===--------------------------------------------------------------------===//

  Expr parseExpr() {
    Expr Result;

    // Literals
    if (Lex.at(Token::KwTrue)) {
      Result.Kind = Expr::EK_True;
      Lex.advance();
    } else if (Lex.at(Token::KwFalse)) {
      Result.Kind = Expr::EK_False;
      Lex.advance();
    } else if (Lex.at(Token::Nat)) {
      Result.Kind = Expr::EK_Nat;
      Result.NumValue = Lex.number();
      Result.Text = Lex.text().str();
      Lex.advance();
    } else if (Lex.at(Token::Hex)) {
      Result.Kind = Expr::EK_Hex;
      Result.NumValue = Lex.number();
      Result.Text = Lex.text().str();
      Lex.advance();
    } else if (Lex.at(Token::Bin)) {
      Result.Kind = Expr::EK_Bin;
      Result.NumValue = Lex.number();
      Result.Text = Lex.text().str();
      Lex.advance();
    } else if (Lex.at(Token::LParen)) {
      // Could be unit literal () or grouped expression
      Result.Kind = Expr::EK_Unit;
      Lex.advance(); // (
      Lex.consume(Token::RParen); // )
    } else if (Lex.at(Token::Op)) {
      // Operator: @op(args) or @op::<N>(args)
      Result.Kind = Expr::EK_Op;
      parseOperatorExpr(Result);
    } else if (Lex.at(Token::KwStruct)) {
      // Struct literal: struct TypeName { field1 = expr1, field2 = expr2 }
      Lex.advance(); // consume 'struct'
      Result.Kind = Expr::EK_Call; // Treat as constructor call
      Result.Text = Lex.text().str(); // Type name
      Lex.advance();
      // Parse brace-enclosed field assignments
      if (Lex.consume(Token::LBrace)) {
        while (!Lex.at(Token::RBrace) && !Lex.atEnd()) {
          if (Lex.at(Token::Ident)) {
            Lex.advance(); // field name (we ignore it, just need the value)
            if (Lex.consume(Token::Eq)) {
              Result.Args.push_back(parseExpr());
            }
          }
          Lex.consume(Token::Comma);
        }
        Lex.consume(Token::RBrace);
      }
    } else if (Lex.at(Token::Ident)) {
      Result.Kind = Expr::EK_Ident;
      Result.Text = Lex.text().str();
      Lex.advance();
      // Check for function call: id(args)
      if (Lex.at(Token::LParen)) {
        Result.Kind = Expr::EK_Call;
        Result.Args = parseArgList();
      }
    } else {
      // Unknown - return empty ident
      Result.Kind = Expr::EK_Ident;
      Result.Text = "";
    }

    // Postfix: .field, is id, as id
    while (true) {
      if (Lex.at(Token::Dot)) {
        Lex.advance();
        Expr FieldExpr;
        FieldExpr.Kind = Expr::EK_Field;
        FieldExpr.Args.push_back(std::move(Result));
        FieldExpr.Text = Lex.text().str();
        Lex.advance();
        Result = std::move(FieldExpr);
      } else if (Lex.at(Token::KwIs)) {
        Lex.advance();
        Expr IsExpr;
        IsExpr.Kind = Expr::EK_Is;
        IsExpr.Args.push_back(std::move(Result));
        IsExpr.Text = Lex.text().str();
        Lex.advance();
        Result = std::move(IsExpr);
      } else if (Lex.at(Token::KwAs)) {
        Lex.advance();
        Expr AsExpr;
        AsExpr.Kind = Expr::EK_As;
        AsExpr.Args.push_back(std::move(Result));
        AsExpr.Text = Lex.text().str();
        Lex.advance();
        Result = std::move(AsExpr);
      } else {
        break;
      }
    }

    return Result;
  }

  void parseOperatorExpr(Expr &Result) {
    StringRef OpText = Lex.text();
    // Parse @op or @op::<N>
    size_t ColonPos = OpText.find("::<");
    if (ColonPos != StringRef::npos) {
      Result.OperatorName = OpText.substr(1, ColonPos - 1).str();
      size_t EndPos = OpText.find('>');
      if (EndPos != StringRef::npos) {
        StringRef WidthStr = OpText.substr(ColonPos + 3, EndPos - ColonPos - 3);
        WidthStr.getAsInteger(10, Result.OperatorWidth);
      }
    } else {
      Result.OperatorName = OpText.substr(1).str();
    }
    Lex.advance();
    Result.Args = parseArgList();
  }

  std::vector<Expr> parseArgList() {
    std::vector<Expr> Args;
    if (!Lex.consume(Token::LParen))
      return Args;
    if (!Lex.at(Token::RParen)) {
      Args.push_back(parseExpr());
      while (Lex.consume(Token::Comma))
        Args.push_back(parseExpr());
    }
    Lex.consume(Token::RParen);
    return Args;
  }

  //===--------------------------------------------------------------------===//
  // Instruction Parsing
  //===--------------------------------------------------------------------===//

  Instr parseInstr() {
    Instr Result;
    Result.LineNumber = CurrentLineNumber++;

    if (Lex.at(Token::KwJump)) {
      Result.Kind = Instr::IK_Jump;
      Lex.advance();
      Result.Value = parseExpr();
      Lex.consume(Token::KwGoto);
      if (Lex.at(Token::Nat)) {
        Result.Target = Lex.number();
        Lex.advance();
      }
    } else if (Lex.at(Token::KwGoto)) {
      Result.Kind = Instr::IK_Goto;
      Lex.advance();
      if (Lex.at(Token::Nat)) {
        Result.Target = Lex.number();
        Lex.advance();
      }
    } else if (Lex.at(Token::KwEnd)) {
      Result.Kind = Instr::IK_End;
      Lex.advance();
    } else if (Lex.at(Token::KwReturn)) {
      Result.Kind = Instr::IK_Return;
      Lex.advance();
      if (Lex.consume(Token::Eq))
        Result.Value = parseExpr();
    } else if (Lex.at(Token::Ident)) {
      std::string Name = Lex.text().str();
      Lex.advance();

      if (Lex.at(Token::Colon)) {
        // Declaration or init: name : Type [= Expr]
        Lex.advance();
        Type Ty = parseType();
        if (Lex.consume(Token::Eq)) {
          Result.Kind = Instr::IK_Init;
          Result.Name = Name;
          Result.Ty = Ty;
          Result.Value = parseExpr();
        } else {
          Result.Kind = Instr::IK_Decl;
          Result.Name = Name;
          Result.Ty = Ty;
        }
      } else if (Lex.consume(Token::Eq)) {
        // Copy: name = Expr
        Result.Kind = Instr::IK_Copy;
        Result.Name = Name;
        Result.Value = parseExpr();
      }
    }

    Lex.consume(Token::Semi);
    return Result;
  }

  //===--------------------------------------------------------------------===//
  // Top-Level Definition Parsing
  //===--------------------------------------------------------------------===//

  RegisterDef parseRegister() {
    RegisterDef Result;
    Lex.advance(); // register
    Result.Name = Lex.text().str();
    Lex.advance();
    Lex.consume(Token::Colon);
    Result.Ty = parseType();
    return Result;
  }

  EnumDef parseEnum() {
    EnumDef Result;
    Lex.advance(); // enum
    Result.Name = Lex.text().str();
    Lex.advance();
    Lex.consume(Token::LBrace);
    while (!Lex.at(Token::RBrace) && !Lex.atEnd()) {
      if (Lex.at(Token::Ident)) {
        Result.Variants.push_back(Lex.text().str());
        Lex.advance();
      }
      Lex.consume(Token::Comma);
    }
    Lex.consume(Token::RBrace);
    return Result;
  }

  UnionDef parseUnion() {
    UnionDef Result;
    Lex.advance(); // union
    Result.Name = Lex.text().str();
    Lex.advance();
    Lex.consume(Token::LBrace);
    while (!Lex.at(Token::RBrace) && !Lex.atEnd()) {
      if (Lex.at(Token::Ident)) {
        std::string Name = Lex.text().str();
        Lex.advance();
        if (!Lex.consume(Token::Colon)) {
          continue; // Skip invalid variant
        }
        Type Ty = parseType();
        Result.Variants.emplace_back(Name, Ty);
      } else {
        Lex.advance(); // Skip unexpected token
      }
      Lex.consume(Token::Comma);
    }
    Lex.consume(Token::RBrace);
    return Result;
  }

  ValDecl parseVal() {
    ValDecl Result;
    Lex.advance(); // val
    Result.Name = Lex.text().str();
    Lex.advance();

    // Check for external: val id = "name" : ...
    if (Lex.consume(Token::Eq)) {
      if (Lex.at(Token::String)) {
        Result.ExternalName = Lex.text().str();
        Lex.advance();
      }
    }

    Lex.consume(Token::Colon);

    // Parse (params) -> return
    if (Lex.consume(Token::LParen)) {
      while (!Lex.at(Token::RParen) && !Lex.atEnd()) {
        Result.ParamTypes.push_back(parseType());
        Lex.consume(Token::Comma);
      }
      Lex.consume(Token::RParen);
    }
    Lex.consume(Token::Arrow);
    Result.ReturnType = parseType();

    return Result;
  }

  FnDef parseFunction() {
    FnDef Result;
    Lex.advance(); // fn
    Result.Name = Lex.text().str();
    Lex.advance();

    Lex.consume(Token::LParen);
    while (!Lex.at(Token::RParen) && !Lex.atEnd()) {
      if (Lex.at(Token::Ident)) {
        Result.ParamNames.push_back(Lex.text().str());
        Lex.advance();
      }
      Lex.consume(Token::Comma);
    }
    Lex.consume(Token::RParen);

    // Parse body
    Lex.consume(Token::LBrace);
    CurrentLineNumber = 0;
    while (!Lex.at(Token::RBrace) && !Lex.atEnd()) {
      Result.Body.push_back(parseInstr());
    }
    Lex.consume(Token::RBrace);

    return Result;
  }

  LetDef parseLet() {
    LetDef Result;
    Lex.advance(); // let
    Lex.consume(Token::LParen);
    Result.Name = Lex.text().str();
    Lex.advance();
    Lex.consume(Token::Colon);
    Result.Ty = parseType();
    Lex.consume(Token::RParen);
    Lex.consume(Token::LBrace);
    CurrentLineNumber = 0;
    while (!Lex.at(Token::RBrace) && !Lex.atEnd()) {
      Result.Body.push_back(parseInstr());
    }
    Lex.consume(Token::RBrace);
    return Result;
  }
};

} // namespace emu
} // namespace llvm

#endif // LLVM_UTILS_TABLEGEN_EMULATORPARSER_H
