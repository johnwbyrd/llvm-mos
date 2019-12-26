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

namespace llvm {

class MOSOperand : public MCParsedAsmOperand {
public:
  MOSOperand() = delete;
  /// Create an immediate MOSOperand.
  MOSOperand(const MCExpr *Val, SMLoc S, SMLoc E)
      : Kind(k_Immediate), Imm(Val), Start(S), End(E){};
  /// Create a register MOSOperand.
  MOSOperand(unsigned RegNum, SMLoc S, SMLoc E)
      : Kind(k_Register), Reg(RegNum), Start(S), End(E){};
  /// Create a token MOSOperand.
  MOSOperand(StringRef Str, SMLoc S) : Kind(k_Token), Tok(Str), Start(S){};

private:
  enum KindTy {
    k_None,
    k_Immediate,
    k_Register,
    k_Token,
  } Kind{k_None};

  MCExpr const *Imm{};
  unsigned int Reg{};
  StringRef Tok;
  SMLoc Start, End;

public:
  template <int64_t Low, int64_t High> bool isImmediate() const {
    if (!isImm()) {
      return false;
    }
    const auto *CE = dyn_cast<MCConstantExpr>(getImm());
    if (!CE) {
      return false;
    }
    int64_t Value = CE->getValue();
    return Value >= Low && Value <= High;
  }

  virtual bool is(KindTy K) const { return (Kind == K); }
  bool isToken() const override { return is(k_Token); }
  bool isImm() const override { return is(k_Immediate); }
  bool isReg() const override { return is(k_Register); }
  bool isMem() const override {
    assert(false);
    return false;
  }
  SMLoc getStartLoc() const override { return Start; }
  SMLoc getEndLoc() const override { return End; }
  const StringRef &getToken() const {
    assert(isToken());
    return Tok;
  }
  const MCExpr *getImm() const {
    assert(isImm());
    return Imm;
  };
  unsigned getReg() const override {
    assert(isReg());
    return Reg;
  }

  virtual bool isImm8() const { return isImmediate<0, 0x100 - 1>(); }
  virtual bool isImm16() const { return isImmediate<0, 0x10000 - 1>(); }
  virtual bool isImm8To16() const { return (!isImm8() && isImm16()); }
  static void addExpr(MCInst &Inst, const MCExpr *Expr) {
    if (const auto *CE = dyn_cast<MCConstantExpr>(Expr)) {
      Inst.addOperand(MCOperand::createImm(CE->getValue()));
    } else {
      Inst.addOperand(MCOperand::createExpr(Expr));
    }
  }

  void addImmOperands(MCInst &Inst, unsigned N) const {
    assert(Kind == k_Immediate && "Unexpected operand kind");
    assert(N == 1 && "Invalid number of operands!");

    const MCExpr *Expr = getImm();
    addExpr(Inst, Expr);
  }

  void addRegOperands(MCInst &Inst, unsigned /*N*/) const {
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  static std::unique_ptr<MOSOperand> createImm(const MCExpr *Val, SMLoc S,
                                               SMLoc E) {
    return make_unique<MOSOperand>(Val, S, E);
  }

  static std::unique_ptr<MOSOperand> createReg(unsigned RegNum, SMLoc S,
                                               SMLoc E) {
    return make_unique<MOSOperand>(RegNum, S, E);
  }

  static std::unique_ptr<MOSOperand> createToken(StringRef Str, SMLoc S) {
    return make_unique<MOSOperand>(Str, S);
  }

  void print(raw_ostream &O) const override {
    switch (Kind) {
    case k_None:
      O << "None";
      break;
    case k_Token:
      O << "Token: \"" << getToken() << "\"";
      break;
    case k_Register:
      O << "Register: " << getReg();
      break;
    case k_Immediate:
      O << "Immediate: \"" << *getImm() << "\"";
      break;
    }
    O << "\n";
  };
};

/// Parses MOS assembly from a stream.
class MOSAsmParser : public MCTargetAsmParser {
  const MCSubtargetInfo &STI;
  MCAsmParser &Parser;
  const MCRegisterInfo *MRI;

#define GET_ASSEMBLER_HEADER
#include "MOSGenAsmMatcher.inc"

public:
  enum MOSMatchResultTy {
    Match_UnknownError = FIRST_TARGET_MATCH_RESULT_TY,
#define GET_OPERAND_DIAGNOSTIC_TYPES
#include "MOSGenAsmMatcher.inc"

  };

  MOSAsmParser(const MCSubtargetInfo &STI, MCAsmParser &Parser,
               const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII), STI(STI), Parser(Parser) {
    MCAsmParserExtension::Initialize(Parser);
    MRI = getContext().getRegisterInfo();

    setAvailableFeatures(ComputeAvailableFeatures(STI.getFeatureBits()));
  }
  MCAsmLexer &getLexer() const { return Parser.getLexer(); }
  MCAsmParser &getParser() const { return Parser; }

  // #define GET_REGISTER_MATCHER
  // #include "MOSGenAsmMatcher.inc"

  bool invalidOperand(SMLoc const &Loc, OperandVector const &Operands,
                      uint64_t const &ErrorInfo) {
    SMLoc ErrorLoc = Loc;
    char const *Diag = nullptr;

    if (ErrorInfo != ~0U) {
      if (ErrorInfo >= Operands.size()) {
        Diag = "too few operands for instruction";
      } else {
        auto const &Op = dynamic_cast<MOSOperand const &>(*Operands[ErrorInfo]);
        if (Op.getStartLoc() != SMLoc()) {
          ErrorLoc = Op.getStartLoc();
        }
      }
    }

    if (Diag == nullptr) {
      Diag = "invalid operand for instruction";
    }

    return Error(ErrorLoc, Diag);
  }

  bool missingFeature(llvm::SMLoc const &Loc, uint64_t const & /*ErrorInfo*/) {
    return Error(Loc,
                 "instruction requires a CPU feature not currently enabled");
  }

  bool emit(MCInst &Inst, SMLoc const &Loc, MCStreamer &Out) const {
    Inst.setLoc(Loc);
    Out.EmitInstruction(Inst, STI);

    return false;
  }

  /// MatchAndEmitInstruction - Recognize a series of operands of a parsed
  /// instruction as an actual MCInst and emit it to the specified MCStreamer.
  /// This returns false on success and returns true on failure to match.
  ///
  /// On failure, the target parser is responsible for emitting a diagnostic
  /// explaining the match failure.
  bool MatchAndEmitInstruction(SMLoc Loc, unsigned & /*Opcode*/,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override {
    MCInst Inst;
    unsigned MatchResult =
        // we always want ConvertToMapAndConstraints to be called
        MatchInstructionImpl(Operands, Inst, ErrorInfo, MatchingInlineAsm);
    switch (MatchResult) {
    case Match_Success:
      return emit(Inst, Loc, Out);
    case Match_MissingFeature:
      return missingFeature(Loc, ErrorInfo);
    case Match_InvalidOperand:
      return invalidOperand(Loc, Operands, ErrorInfo);
    case Match_MnemonicFail:
      return Error(Loc, "invalid instruction");
    case Match_InvalidImm8:
      return Error(Loc, "operand must be an 8-bit value (less than 256)");
    case Match_InvalidImm16:
      return Error(Loc, "operand must be an 16-bit value (less than 65536)");
    case Match_InvalidImm8To16:
      return Error(Loc, "operand must be an 8 to 16 bit value (between 256 and "
                        "65535 inclusive)");
    case Match_NearMisses:
      return Error(Loc, "found some near misses");
    default:
      return true;
    }
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
  bool ParseDirective(AsmToken /*DirectiveID*/) override {
    // todo
    return true;
  }

  virtual signed char hexToChar(const signed char Letter) {
    if (Letter >= '0' && Letter <= '9') {
      return static_cast<signed char>(Letter - '0');
    }
    if (Letter >= 'a' && Letter <= 'f') {
      return static_cast<signed char>(Letter - 'a' + 10);
    }
    return -1;
  }

  // Converts what could be a hex string to an integer value.
  // Result must fit into 32 bits.  More than that is an error.
  // Like everything else in this particular API, it returns false on success.
  virtual bool tokenToHex(uint64_t &Res, const AsmToken &Tok) {
    Res = 0;
    std::string Text = Tok.getString();
    if (Text.size() > 8) {
      return true;
    }
    std::transform(Text.begin(), Text.end(), Text.begin(),
                   [](unsigned char C) { return std::tolower(C); });
    for (char Letter : Text) {
      signed char Converted = hexToChar(Letter);
      if (Converted == -1) {
        return true;
      }
      Res = (Res * 16) + Converted;
    }
    return false;
  }

  /// On MOS, the dollar sign is a prefix for a hex number.  We handle
  /// this as a special case of expression parsing, so that the user
  /// can do math and such on MOS hexadecimal numbers.
  bool parsePrimaryExpr(const MCExpr *&Res, SMLoc &EndLoc) override {
    AsmToken::TokenKind FirstTokenKind = getLexer().getKind();
    if (FirstTokenKind == AsmToken::TokenKind::Dollar) {
      uint64_t Value = 0;
      if (tokenToHex(Value, getLexer().peekTok())) {
        return true;
      }
      // we've successfully got two tokens which can collectively be
      // understood as a constant integer value
      Lex();
      Lex();
      Res = MCConstantExpr::create(Value, getContext(), true);
      return false;
    }
    return getParser().parsePrimaryExpr(Res, EndLoc);
  }

  bool ParseInstruction(ParseInstructionInfo & /*Info*/, StringRef Mnemonic,
                        SMLoc NameLoc, OperandVector &Operands) override {
    /*
    On 65xx family instructions, mnemonics and addressing modes take the form:

    mnemonic (#)expr
    mnemonic [(]expr[),xy]*

    65816 only:
    mnemonic [(]expr[),sxy]*
    mnemonic \[ expr \]

    Any constant may be prefixed by a $, indicating that it is a hex constant.
    Such onstants can appear anywhere an integer appears in an expr, so expr
    parsing needs to take that into account.

    Handle all these cases, fairly loosely, and let tablegen sort out what's
    what.

    */
    // First, the mnemonic goes on the stack.
    Operands.push_back(MOSOperand::createToken(Mnemonic, NameLoc));
#if 0
    std::vector<AsmToken> Tokens;
    const size_t MaxTokensPerLine = 256;
    Tokens.resize(MaxTokensPerLine);
    getLexer().peekTokens(Tokens);
    for (size_t T = 0; T < MaxTokensPerLine; T++) {
      if (Tokens[T].is(AsmToken::EndOfStatement)) {
        Tokens.resize(T);
        break;
      }
    }
#endif

    while (getLexer().isNot(AsmToken::EndOfStatement)) {
      if (getLexer().is(AsmToken::Hash)) {
        Operands.push_back(MOSOperand::createToken(
            getLexer().getTok().getString(), getLexer().getLoc()));
        Lex();
        MCExpr const *Expression;
        SMLoc S = getLexer().getLoc();
        if (getParser().parseExpression(Expression)) {
          Parser.eatToEndOfStatement();
          return Error(
              getLexer().getLoc(),
              "immediate operand must be an expression evaluating to a "
              "value between 0 and 255 inclusive");
        }
        SMLoc E = getLexer().getTok().getEndLoc();
        Operands.push_back(MOSOperand::createImm(Expression, S, E));
        return false;
      }
      if (!tryParseImmediate(Operands)) {
        continue;
      }

      SMLoc Loc = getLexer().getLoc();
      Parser.eatToEndOfStatement();
      return Error(Loc, "failed to parse register and immediate pair");
    }
    Parser.Lex(); // Consume the EndOfStatement
    return false;
  }

  unsigned int parseRegister() {
    unsigned int RegNum = MOS::NoRegister;

    if (Parser.getTok().is(AsmToken::Identifier)) {
      // Check for register pair syntax
      if (Parser.getLexer().peekTok().is(AsmToken::Colon)) {
        Parser.Lex();
        Parser.Lex(); // Eat high (odd) register and colon
        /*

        if (Parser.getTok().is(AsmToken::Identifier)) {
          // Convert lower (even) register to DREG
          RegNum = toDREG(parseRegisterName());
        }
        */
      } else {
        RegNum = parseRegisterName();
      }
    }
    return RegNum;
  }

  bool ParseRegister(unsigned & /*RegNo*/, SMLoc & /*StartLoc*/,
                     SMLoc & /*EndLoc*/) override {
    // todo
    return true;
  }

  unsigned int parseRegisterName(unsigned (*MatchFn)(StringRef)) {
    StringRef Name = Parser.getTok().getString();

    unsigned int RegNum = MatchFn(Name);

    if (RegNum == MOS::NoRegister) {
      RegNum = MatchFn(Name.lower());
    }
    if (RegNum == MOS::NoRegister) {
      RegNum = MatchFn(Name.upper());
    }

    return RegNum;
  }

  unsigned int parseRegisterName();

  bool tryParseImmediate(OperandVector & /*Operands*/) {
    if (Parser.getTok().is(AsmToken::Hash)) {
      Lex();
      // AsmToken const &T = Parser.getTok();

      /*
          if (T.is(AsmToken::Integer)) {
            Operands.push_back(MOSOperand::CreateImm( T, T.getLoc(),
         T.getEndLoc() ); return false;
          }
      */
    }
    return true;
  }

  bool tryParseRegisterOperand(OperandVector &Operands) {
    unsigned int RegNo = parseRegister();

    if (RegNo == MOS::NoRegister) {
      return true;
    }

    AsmToken const &T = Parser.getTok();
    Operands.push_back(MOSOperand::createReg(RegNo, T.getLoc(), T.getEndLoc()));
    Parser.Lex(); // Eat register token.

    return false;
  }

  bool tryParseExpression(OperandVector &Operands) {
    SMLoc S = Parser.getTok().getLoc();

    if ((Parser.getTok().getKind() == AsmToken::Plus ||
         Parser.getTok().getKind() == AsmToken::Minus) &&
        Parser.getLexer().peekTok().getKind() == AsmToken::Identifier) {
      // Don't handle this case - it should be split into two
      // separate tokens.
      return true;
    }

    // Parse (potentially inner) expression
    MCExpr const *Expression;
    if (getParser().parseExpression(Expression)) {
      return true;
    }

    SMLoc E = SMLoc::getFromPointer(Parser.getTok().getLoc().getPointer());
    Operands.push_back(MOSOperand::createImm(Expression, S, E));
    return false;
  }
}; // class MOSAsmParser
#define GET_MATCHER_IMPLEMENTATION
// #define GET_SUBTARGET_FEATURE_NAME
#define GET_REGISTER_MATCHER
// #define GET_MNEMONIC_SPELL_CHECKER
#include "MOSGenAsmMatcher.inc"

unsigned int MOSAsmParser::parseRegisterName() {
  unsigned int RegNum = parseRegisterName(&MatchRegisterName);
  if (RegNum == MOS::NoRegister) {
    RegNum = parseRegisterName(&MatchRegisterAltName);
  }
  return RegNum;
}

extern "C" void LLVMInitializeMOSAsmParser() { // NOLINT
  RegisterMCAsmParser<MOSAsmParser> X(getTheMOSTarget());
}

} // namespace llvm
