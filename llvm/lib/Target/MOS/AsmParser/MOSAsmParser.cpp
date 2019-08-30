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
  MCAsmLexer &getLexer() const { return Parser.getLexer(); }
  MCAsmParser &getParser() const { return Parser; }
  /// MatchAndEmitInstruction - Recognize a series of operands of a parsed
  /// instruction as an actual MCInst and emit it to the specified MCStreamer.
  /// This returns false on success and returns true on failure to match.
  ///
  /// On failure, the target parser is responsible for emitting a diagnostic
  /// explaining the match failure.
  virtual bool MatchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                                       OperandVector &Operands, MCStreamer &Out,
                                       uint64_t &ErrorInfo,
                                       bool MatchingInlineAsm) override {
    // todo
    return true;
  }

  /// ParseDirective - Parse a target specific assembler directive
  ///
  /// The parser is positioned following the directive name.  The target
  /// specific directive parser should parse the entire directive doing or
  /// recording any target specific work, or return true and do nothing if the
  /// directive is not target specific. If the directive is specific for
  /// the target, the entire line is parsed up to and including the
  /// end-of-statement token and false is returned.
  ///
  /// \param DirectiveID - the identifier token of the directive.
  virtual bool ParseDirective(AsmToken DirectiveID) override {
    // todo
    return true;
  }

  virtual bool ParseInstruction(ParseInstructionInfo &Info, StringRef Name,
                                SMLoc NameLoc,
                                OperandVector &Operands) override {
    // todo
    return true;
  }

  virtual bool ParseRegister(unsigned &RegNo, SMLoc &StartLoc,
                             SMLoc &EndLoc) override {
    // todo
    return true;
  }

};

/// An parsed MOS assembly operand.
class MOSOperand : public MCParsedAsmOperand {
public:
  typedef MCParsedAsmOperand Base;
  void addRegOperands(MCInst &Inst, unsigned N) const {
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  /// getStartLoc - Get the location of the first token of this operand.
  virtual SMLoc getStartLoc() const { return SMLoc(); }
  /// getEndLoc - Get the location of the last token of this operand.
  virtual SMLoc getEndLoc() const { return SMLoc(); }

  StringRef getToken() const { return Tok; }

  virtual unsigned getReg() const { return 0; }

  /// isImm - Is this an immediate operand?
  virtual bool isImm() const { return false; }
  /// isReg - Is this a register operand?
  virtual bool isReg() const { return false; }
  /// isMem - Is this a memory operand?
  virtual bool isMem() const { return false; }

  /// isToken - Is this a token operand?
  virtual bool isToken() const { return false; }
  /// print - Print a debug representation of the operand to the given stream.
  virtual void print(raw_ostream &OS) const {
    // todo
  }

protected:
  StringRef Tok;
};

extern "C" void LLVMInitializeMOSAsmParser() {
  RegisterMCAsmParser<MOSAsmParser> X(getTheMOSTarget());
}

#define GET_REGISTER_MATCHER
#define GET_MATCHER_IMPLEMENTATION
#include "MOSGenAsmMatcher.inc"

} // namespace llvm
