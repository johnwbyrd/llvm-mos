//===---- MOSAsmParser.cpp - Parse MOS assembly to MCInst instructions ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/MOSMCELFStreamer.h"
#include "MCTargetDesc/MOSMCExpr.h"
#include "MCTargetDesc/MOSMCTargetDesc.h"
#include "MOS.h"
#include "MOSRegisterInfo.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCParser/MCAsmLexer.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/TargetRegistry.h"

#include <sstream>

#define DEBUG_TYPE "mos-asm-parser"

using namespace llvm;

namespace llvm {
/// Parses MOS assembly from a stream.
class MOSAsmParser : public MCTargetAsmParser {
  const MCSubtargetInfo &STI;
  MCAsmParser &Parser;
  const MCRegisterInfo *MRI;

#define GET_ASSEMBLER_HEADER
#include "MOSGenAsmMatcher.inc"

public:
  MOSAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
               const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), STI(STI), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    MRI = getContext().getRegisterInfo();

    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }

  MCAsmParser &getParser() const { return Parser; }
  MCAsmLexer &getLexer() const { return Parser.getLexer(); }
};

/// An parsed MOS assembly operand.
class MOSOperand : public MCParsedAsmOperand {
  typedef MCParsedAsmOperand Base;
  /// isToken - Is this a token operand?
  virtual bool isToken() const { return false; }
  /// isImm - Is this an immediate operand?
  virtual bool isImm() const { return false; }
  /// isReg - Is this a register operand?
  virtual bool isReg() const { return false; }
  virtual unsigned getReg() const { return 0; }

  /// isMem - Is this a memory operand?
  virtual bool isMem() const { return false; }

  /// getStartLoc - Get the location of the first token of this operand.
  virtual SMLoc getStartLoc() const { return SMLoc(); }
  /// getEndLoc - Get the location of the last token of this operand.
  virtual SMLoc getEndLoc() const { return SMLoc(); }

  /// print - Print a debug representation of the operand to the given stream.
  virtual void print(raw_ostream &OS) const {
    // todo
  }
};

extern "C" void LLVMInitializeAVRAsmParser() {
  RegisterMCAsmParser<MOSAsmParser> X(getTheMOSTarget());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "MOSGenAsmMatcher.inc"



} // namespace llvm
