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
//   SAIL IR Text --> Lexer --> Parser --> AST --> CodeGen --> C++
//
// The input is a SAIL Jib IR file (produced by the SAIL compiler).
// The output is C++ code organized into preprocessor-guarded sections:
//
//   GET_EMULATOR_TYPES   - Union/struct type definitions (namespace scope)
//   GET_EMULATOR_MEMBERS - Member variable declarations (class scope)
//   GET_EMULATOR_METHODS - Helper method definitions (class scope)
//   GET_EMULATOR_MAPPING - MCInst to SAIL instruction mapping function
//
// Usage:
//   llvm-tblgen -gen-emulator -sail-ir=<path-to-ir> <tablegen-file>
//
// Components (in separate headers):
//   EmulatorLexer.h   - Token types and lexer
//   EmulatorAST.h     - AST node types
//   EmulatorParser.h  - Recursive descent parser
//   EmulatorCodeGen.h - C++ code generator
//
//===----------------------------------------------------------------------===//

#include "EmulatorCodeGen.h"
#include "EmulatorLexer.h"
#include "EmulatorParser.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/TableGen/Error.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"

using namespace llvm;

static cl::opt<std::string>
    SailIRFile("sail-ir", cl::desc("Path to SAIL Jib IR file"), cl::init(""));

static cl::opt<bool>
    DebugLexer("debug-lexer", cl::desc("Debug lexer token stream"), cl::init(false));

static cl::opt<bool>
    DebugParser("debug-parser", cl::desc("Debug parser progress"), cl::init(false));

static cl::opt<unsigned>
    DebugMaxTokens("debug-max-tokens",
                   cl::desc("Maximum tokens to process (0=unlimited)"),
                   cl::init(0));

namespace {

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
    emu::Lexer Lex((*BufOrErr)->getBuffer());
    if (DebugLexer || DebugMaxTokens > 0)
      Lex.setDebug(DebugLexer, DebugMaxTokens);

    emu::Parser Parse(Lex);
    if (DebugParser)
      Parse.setDebug(true);

    emu::JibIR IR = Parse.parse();

    if (DebugLexer || DebugParser) {
      errs() << "DEBUG: Parsing complete. Tokens processed: "
             << Lex.getTokenCount() << "\n";
      errs() << "DEBUG: Functions: " << IR.Functions.size()
             << ", Types: " << IR.Types.size()
             << ", Vals: " << IR.Vals.size() << "\n";
    }

    // Generate
    emu::CodeGen CG(IR, OS);
    CG.emit();
  }
};

} // end anonymous namespace

static TableGen::Emitter::OptClass<EmulatorEmitter>
    X("gen-emulator", "Generate instruction emulator from SAIL IR");
