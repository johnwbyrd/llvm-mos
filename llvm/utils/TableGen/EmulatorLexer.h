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
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
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
  KwStruct,
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
  bool DebugEnabled = false;
  uint64_t TokenCount = 0;
  uint64_t MaxTokens = 0;

public:
  explicit Lexer(StringRef Source) : Input(Source) { advance(); }

  /// Enable debug output for lexer.
  void setDebug(bool Enable, uint64_t MaxToks = 0) {
    DebugEnabled = Enable;
    MaxTokens = MaxToks;
  }

  /// Get the number of tokens processed.
  uint64_t getTokenCount() const { return TokenCount; }

  /// Get current position in input.
  size_t getPosition() const { return Position; }

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

  /// Get string name for a token type (for debugging).
  static const char *tokenName(Token T) {
    switch (T) {
    case Token::Ident: return "Ident";
    case Token::Nat: return "Nat";
    case Token::Hex: return "Hex";
    case Token::Bin: return "Bin";
    case Token::String: return "String";
    case Token::KwFn: return "KwFn";
    case Token::KwVal: return "KwVal";
    case Token::KwEnum: return "KwEnum";
    case Token::KwUnion: return "KwUnion";
    case Token::KwStruct: return "KwStruct";
    case Token::KwRegister: return "KwRegister";
    case Token::KwLet: return "KwLet";
    case Token::KwJump: return "KwJump";
    case Token::KwGoto: return "KwGoto";
    case Token::KwEnd: return "KwEnd";
    case Token::KwTrue: return "KwTrue";
    case Token::KwFalse: return "KwFalse";
    case Token::KwIs: return "KwIs";
    case Token::KwAs: return "KwAs";
    case Token::KwReturn: return "KwReturn";
    case Token::TyI: return "TyI";
    case Token::TyI64: return "TyI64";
    case Token::TyBv: return "TyBv";
    case Token::TyUnit: return "TyUnit";
    case Token::TyBool: return "TyBool";
    case Token::TyEnum: return "TyEnum";
    case Token::TyStruct: return "TyStruct";
    case Token::Op: return "Op";
    case Token::LParen: return "LParen";
    case Token::RParen: return "RParen";
    case Token::LBrace: return "LBrace";
    case Token::RBrace: return "RBrace";
    case Token::Colon: return "Colon";
    case Token::Eq: return "Eq";
    case Token::Comma: return "Comma";
    case Token::Semi: return "Semi";
    case Token::Arrow: return "Arrow";
    case Token::Dot: return "Dot";
    case Token::Eof: return "Eof";
    case Token::Error: return "Error";
    }
    return "Unknown";
  }

  /// Advance to the next token and return its type.
  Token advance() {
    // Use a loop instead of recursion to avoid stack overflow
    while (true) {
      skipWhitespaceAndComments();
      if (Position >= Input.size()) {
        CurrentToken = Token::Eof;
        break;
      }

      // Check token limit for debugging
      if (MaxTokens > 0 && TokenCount >= MaxTokens) {
        errs() << "DEBUG: Token limit reached (" << MaxTokens << ")\n";
        errs() << "DEBUG: Position=" << Position << " of " << Input.size() << "\n";
        CurrentToken = Token::Eof;
        break;
      }
      ++TokenCount;

      char C = Input[Position];

      // Single-character tokens
      switch (C) {
      case '(':
        ++Position;
        CurrentToken = Token::LParen;
        goto done;
      case ')':
        ++Position;
        CurrentToken = Token::RParen;
        goto done;
      case '{':
        ++Position;
        CurrentToken = Token::LBrace;
        goto done;
      case '}':
        ++Position;
        CurrentToken = Token::RBrace;
        goto done;
      case ':':
        ++Position;
        CurrentToken = Token::Colon;
        goto done;
      case ',':
        ++Position;
        CurrentToken = Token::Comma;
        goto done;
      case ';':
        ++Position;
        CurrentToken = Token::Semi;
        goto done;
      case '.':
        ++Position;
        CurrentToken = Token::Dot;
        goto done;
      case '=':
        ++Position;
        CurrentToken = Token::Eq;
        goto done;
      default:
        break;
      }

      // Arrow: ->
      if (C == '-' && Position + 1 < Input.size() && Input[Position + 1] == '>') {
        Position += 2;
        CurrentToken = Token::Arrow;
        break;
      }

      // String literal
      if (C == '"') {
        lexString();
        break;
      }

      // Hex literal: 0x...
      if (C == '0' && Position + 1 < Input.size() && Input[Position + 1] == 'x') {
        lexHex();
        break;
      }

      // Binary literal: 0b...
      if (C == '0' && Position + 1 < Input.size() && Input[Position + 1] == 'b') {
        lexBinary();
        break;
      }

      // Decimal number (including negative)
      if (isdigit(C) ||
          (C == '-' && Position + 1 < Input.size() &&
           isdigit(Input[Position + 1]))) {
        lexNumber();
        break;
      }

      // Type annotation: %type
      if (C == '%') {
        lexType();
        break;
      }

      // Operator: @op
      if (C == '@') {
        lexOperator();
        break;
      }

      // Source location backtick: skip to end of annotation and continue loop
      if (C == '`') {
        while (Position < Input.size() && Input[Position] != '\n' &&
               Input[Position] != ';')
          ++Position;
        continue; // Loop again instead of recursion
      }

      // Identifier or keyword
      if (isalpha(C) || C == '_' || C == '$') {
        lexIdentOrKeyword();
        break;
      }

      // Unknown character - skip and continue loop
      if (DebugEnabled) {
        errs() << "DEBUG: Skipping unknown char '" << C << "' ("
               << format_hex((unsigned char)C, 4) << ") at position "
               << Position << "\n";
      }
      ++Position;
      continue; // Loop again instead of recursion
    }

  done:
    if (DebugEnabled) {
      errs() << "DEBUG: Token #" << TokenCount << " " << tokenName(CurrentToken)
             << " '" << CurrentText << "' at pos " << Position << "\n";
    }
    return CurrentToken;
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
        .Case("struct", Token::KwStruct)
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
