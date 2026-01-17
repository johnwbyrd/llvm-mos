//===- EmulatorLexer.h - Lexer for SAIL Jib IR ------------------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines the Lexer for SAIL Jib IR. The lexer tokenizes the
// textual IR format produced by the SAIL compiler's Jib backend.
//
// Token types include:
// - Identifiers and keywords (fn, val, enum, union, register, let, etc.)
// - Type annotations (%i, %i64, %bvN, %unit, %bool, %enum, %struct)
// - Operators (@-prefixed: @bvadd, @concat, etc.)
// - Literals (decimal, hex 0x..., binary 0b..., strings, booleans)
// - Punctuation and structural tokens
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_UTILS_TABLEGEN_EMULATORLEXER_H
#define LLVM_UTILS_TABLEGEN_EMULATORLEXER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include <cstdint>
#include <string>

namespace llvm {
namespace emu {

//===----------------------------------------------------------------------===//
// Token Types
//===----------------------------------------------------------------------===//

enum class Token {
  // Literals
  Ident,
  Nat,
  Hex,
  Bin,
  String,

  // Keywords
  KwFn,
  KwVal,
  KwEnum,
  KwUnion,
  KwRegister,
  KwLet,
  KwJump,
  KwGoto,
  KwEnd,
  KwTrue,
  KwFalse,
  KwIs,
  KwAs,
  KwReturn,

  // Types (%-prefixed)
  TyI,
  TyI64,
  TyBv,
  TyUnit,
  TyBool,
  TyEnum,
  TyStruct,

  // Operators (@-prefixed)
  Op,

  // Punctuation
  LParen,
  RParen,
  LBrace,
  RBrace,
  Colon,
  Eq,
  Comma,
  Semi,
  Arrow,
  Dot,

  // Special
  Eof,
  Error
};

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

/// Lexer for SAIL Jib IR text format.
class Lexer {
  StringRef Input;
  size_t Position = 0;
  Token CurrentToken = Token::Eof;
  std::string CurrentText;
  int64_t CurrentNumber = 0;

public:
  explicit Lexer(StringRef Source) : Input(Source) { advance(); }

  /// Get the current token type.
  Token token() const { return CurrentToken; }

  /// Get the text of the current token.
  StringRef text() const { return CurrentText; }

  /// Get the numeric value of the current token (for numeric literals).
  int64_t number() const { return CurrentNumber; }

  /// Check if current token matches the given type.
  bool at(Token T) const { return CurrentToken == T; }

  /// Check if we've reached end of input.
  bool atEnd() const { return CurrentToken == Token::Eof; }

  /// Consume the current token if it matches, return true if consumed.
  bool consume(Token T) {
    if (CurrentToken == T) {
      advance();
      return true;
    }
    return false;
  }

  /// Advance to the next token and return its type.
  Token advance() {
    skipWhitespaceAndComments();
    if (Position >= Input.size())
      return CurrentToken = Token::Eof;

    char C = Input[Position];

    // Single-character tokens
    switch (C) {
    case '(': ++Position; return CurrentToken = Token::LParen;
    case ')': ++Position; return CurrentToken = Token::RParen;
    case '{': ++Position; return CurrentToken = Token::LBrace;
    case '}': ++Position; return CurrentToken = Token::RBrace;
    case ':': ++Position; return CurrentToken = Token::Colon;
    case ',': ++Position; return CurrentToken = Token::Comma;
    case ';': ++Position; return CurrentToken = Token::Semi;
    case '.': ++Position; return CurrentToken = Token::Dot;
    case '=': ++Position; return CurrentToken = Token::Eq;
    default: break;
    }

    // Arrow: ->
    if (C == '-' && Position + 1 < Input.size() && Input[Position + 1] == '>') {
      Position += 2;
      return CurrentToken = Token::Arrow;
    }

    // String literal
    if (C == '"')
      return lexString();

    // Hex literal: 0x...
    if (C == '0' && Position + 1 < Input.size() && Input[Position + 1] == 'x')
      return lexHex();

    // Binary literal: 0b...
    if (C == '0' && Position + 1 < Input.size() && Input[Position + 1] == 'b')
      return lexBinary();

    // Decimal number (including negative)
    if (isdigit(C) ||
        (C == '-' && Position + 1 < Input.size() && isdigit(Input[Position + 1])))
      return lexNumber();

    // Type annotation: %type
    if (C == '%')
      return lexType();

    // Operator: @op
    if (C == '@')
      return lexOperator();

    // Source location backtick: skip to end
    if (C == '`') {
      while (Position < Input.size() && Input[Position] != '\n' &&
             Input[Position] != ';')
        ++Position;
      return advance();
    }

    // Identifier or keyword
    if (isalpha(C) || C == '_' || C == '$')
      return lexIdentOrKeyword();

    // Unknown character - skip and try again
    ++Position;
    return advance();
  }

private:
  void skipWhitespaceAndComments() {
    while (Position < Input.size()) {
      char C = Input[Position];
      if (C == ' ' || C == '\t' || C == '\n' || C == '\r') {
        ++Position;
        continue;
      }
      // Skip // comments
      if (C == '/' && Position + 1 < Input.size() && Input[Position + 1] == '/') {
        while (Position < Input.size() && Input[Position] != '\n')
          ++Position;
        continue;
      }
      break;
    }
  }

  Token lexString() {
    ++Position; // Skip opening quote
    size_t Start = Position;
    while (Position < Input.size() && Input[Position] != '"')
      ++Position;
    CurrentText = Input.substr(Start, Position - Start).str();
    if (Position < Input.size())
      ++Position; // Skip closing quote
    return CurrentToken = Token::String;
  }

  Token lexHex() {
    size_t Start = Position;
    Position += 2; // Skip 0x
    while (Position < Input.size() && isxdigit(Input[Position]))
      ++Position;
    CurrentText = Input.substr(Start, Position - Start).str();
    CurrentNumber = std::strtoll(CurrentText.c_str() + 2, nullptr, 16);
    return CurrentToken = Token::Hex;
  }

  Token lexBinary() {
    size_t Start = Position;
    Position += 2; // Skip 0b
    while (Position < Input.size() &&
           (Input[Position] == '0' || Input[Position] == '1'))
      ++Position;
    CurrentText = Input.substr(Start, Position - Start).str();
    CurrentNumber = std::strtoll(CurrentText.c_str() + 2, nullptr, 2);
    return CurrentToken = Token::Bin;
  }

  Token lexNumber() {
    size_t Start = Position;
    if (Input[Position] == '-')
      ++Position;
    while (Position < Input.size() && isdigit(Input[Position]))
      ++Position;
    CurrentText = Input.substr(Start, Position - Start).str();
    CurrentNumber = std::strtoll(CurrentText.c_str(), nullptr, 10);
    return CurrentToken = Token::Nat;
  }

  Token lexType() {
    size_t Start = Position;
    ++Position; // Skip %
    while (Position < Input.size() &&
           (isalnum(Input[Position]) || Input[Position] == '_'))
      ++Position;
    CurrentText = Input.substr(Start, Position - Start).str();
    StringRef TypeStr = CurrentText;

    if (TypeStr == "%i")
      return CurrentToken = Token::TyI;
    if (TypeStr == "%i64")
      return CurrentToken = Token::TyI64;
    if (TypeStr.starts_with("%bv"))
      return CurrentToken = Token::TyBv;
    if (TypeStr == "%unit")
      return CurrentToken = Token::TyUnit;
    if (TypeStr == "%bool")
      return CurrentToken = Token::TyBool;
    if (TypeStr.starts_with("%enum"))
      return CurrentToken = Token::TyEnum;
    if (TypeStr.starts_with("%struct") || TypeStr.starts_with("%union"))
      return CurrentToken = Token::TyStruct;

    return CurrentToken = Token::TyBv; // Default for unknown types
  }

  Token lexOperator() {
    size_t Start = Position;
    ++Position; // Skip @
    // Handle turbofish: @op::<N>
    while (Position < Input.size() &&
           (isalnum(Input[Position]) || Input[Position] == '_' ||
            Input[Position] == ':' || Input[Position] == '<' ||
            Input[Position] == '>'))
      ++Position;
    CurrentText = Input.substr(Start, Position - Start).str();
    return CurrentToken = Token::Op;
  }

  Token lexIdentOrKeyword() {
    size_t Start = Position;
    while (Position < Input.size() &&
           (isalnum(Input[Position]) || Input[Position] == '_' ||
            Input[Position] == '$' || Input[Position] == '\''))
      ++Position;
    CurrentText = Input.substr(Start, Position - Start).str();

    return CurrentToken = StringSwitch<Token>(CurrentText)
        .Case("fn", Token::KwFn)
        .Case("val", Token::KwVal)
        .Case("enum", Token::KwEnum)
        .Case("union", Token::KwUnion)
        .Case("register", Token::KwRegister)
        .Case("let", Token::KwLet)
        .Case("jump", Token::KwJump)
        .Case("goto", Token::KwGoto)
        .Case("end", Token::KwEnd)
        .Case("true", Token::KwTrue)
        .Case("false", Token::KwFalse)
        .Case("is", Token::KwIs)
        .Case("as", Token::KwAs)
        .Case("return", Token::KwReturn)
        .Default(Token::Ident);
  }
};

} // namespace emu
} // namespace llvm

#endif // LLVM_UTILS_TABLEGEN_EMULATORLEXER_H
