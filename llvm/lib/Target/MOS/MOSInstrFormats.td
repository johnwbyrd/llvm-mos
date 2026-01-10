//===-- MOSInstrInfo.td - MOS Instruction Formats ----------*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// MOS Instruction Format Definitions.
//
//===----------------------------------------------------------------------===//

/// Predicates. Useful for limiting instructions to particular hardware modes
/// or particular hardware implementations.
def Has6502 : Predicate<"Subtarget->has6502()">,
              AssemblerPredicate<(all_of Feature6502), "6502"> {
  let PredicateName = "Feature6502";
}

def Has6502X : Predicate<"Subtarget->has6502X()">,
               AssemblerPredicate<(all_of Feature6502X), "6502X"> {
  let PredicateName = "Feature6502X";
}

def Has65C02 : Predicate<"Subtarget->has65C02()">,
               AssemblerPredicate<(all_of Feature65C02), "65C02"> {
  let PredicateName = "Feature65C02";
}

def Has65CE02 : Predicate<"Subtarget->has65CE02()">,
               AssemblerPredicate<(all_of Feature65CE02), "65CE02"> {
  let PredicateName = "Feature65CE02";
}

def HasHUC6280 : Predicate<"Subtarget->hasHUC6280()">,
               AssemblerPredicate<(all_of FeatureHUC6280), "HuC6280"> {
  let PredicateName = "FeatureHUC6280";
}

def HasR65C02 : Predicate<"Subtarget->hasR65C02()">,
               AssemblerPredicate<(all_of FeatureR65C02), "R65C02"> {
  let PredicateName = "FeatureR65C02";
}

def HasW65C02 : Predicate<"Subtarget->hasW65C02()">,
               AssemblerPredicate<(all_of FeatureW65C02), "W65C02"> {
  let PredicateName = "FeatureW65C02";
}

def HasW65816Or65EL02 : Predicate<"(Subtarget->hasW65816() || Subtarget->has65EL02())">,
                AssemblerPredicate<(any_of FeatureW65816, Feature65EL02), "W65816 or 65EL02"> {
  let PredicateName = "FeatureW65816Or65EL02";
}

def HasW65816 : Predicate<"Subtarget->hasW65816()">,
                AssemblerPredicate<(all_of FeatureW65816), "W65816"> {
  let PredicateName = "FeatureW65816";
}

def Has65EL02 : Predicate<"Subtarget->has65EL02()">,
                AssemblerPredicate<(all_of Feature65EL02), "65EL02"> {
  let PredicateName = "Feature65EL02";
}

def Has6502XOrDTV : Predicate<"(Subtarget->has6502X() || Subtarget->has65DTV02())">,
                AssemblerPredicate<(any_of Feature6502X, Feature65DTV02), "6502X (C64DTV subset)"> {
  let PredicateName = "Feature6502XOrDTV";
}

def Has65DTV02 : Predicate<"Subtarget->has65DTV02()">,
                AssemblerPredicate<(all_of Feature65DTV02), "6502 (C64DTV)"> {
  let PredicateName = "Feature65DTV02";
}

def Has4510 : Predicate<"Subtarget->has4510()">,
                AssemblerPredicate<(all_of Feature4510), "4510"> {
  let PredicateName = "Feature4510";
}

def Has45GS02 : Predicate<"Subtarget->has45GS02()">,
                AssemblerPredicate<(all_of Feature45GS02), "45GS02"> {
  let PredicateName = "Feature45GS02";
}

def HasSPC700 : Predicate<"Subtarget->hasSPC700()">,
               AssemblerPredicate<(all_of FeatureSPC700), "SPC700"> {
  let PredicateName = "FeatureSPC700";
}

def HasBRA : Predicate<"Subtarget->hasBRA()">;
def HasGPRStackRegs : Predicate<"Subtarget->hasGPRStackRegs()">;
def HasGPRIncDec : Predicate<"Subtarget->hasGPRIncDec()">;
def HasJMPIdxIndir : Predicate<"Subtarget->hasJMPIdxIndir()">;

/// These instruction format definitions exist, thanks to Neil Parker's
/// analysis of the 65xx instruction set, at:
/// http://nparker.llx.com/a2/opcodes.html
/// Parker's analysis is closely reflected in the structure of this file, and
/// his words are quoted liberally herein.  Parker's sense-making of the 6502
/// and family was critical in creating these format definitions, and we're
/// indebted to him for the excellent analysis.

//===----------------------------------------------------------------------===//
// Flag Effect Constants
//===----------------------------------------------------------------------===//
// 2-bit encoding for how each flag is affected by an instruction:
//   0 = unaffected
//   1 = modified (value depends on operation result)
//   2 = always set to 1
//   3 = always cleared to 0

defvar FLAG_UNAFFECTED = 0;
defvar FLAG_MODIFIED = 1;
defvar FLAG_SET = 2;
defvar FLAG_CLEARED = 3;

//===----------------------------------------------------------------------===//
// Flag Effect Helper Classes
//===----------------------------------------------------------------------===//
// These classes set common flag patterns. Inherit from them to set flags.

// Most load/logic ops affect N and Z based on result
class FlagsNZ {
  bits<2> FlagN = 1;  // FLAG_MODIFIED
  bits<2> FlagZ = 1;  // FLAG_MODIFIED
}

// Comparison affects N, Z, C based on result
class FlagsCMP {
  bits<2> FlagN = 1;  // FLAG_MODIFIED
  bits<2> FlagZ = 1;  // FLAG_MODIFIED
  bits<2> FlagC = 1;  // FLAG_MODIFIED
}

// Arithmetic (ADC/SBC) affects N, V, Z, C based on result
class FlagsArith {
  bits<2> FlagN = 1;  // FLAG_MODIFIED
  bits<2> FlagV = 1;  // FLAG_MODIFIED
  bits<2> FlagZ = 1;  // FLAG_MODIFIED
  bits<2> FlagC = 1;  // FLAG_MODIFIED
}

// Shifts/rotates affect N, Z, C based on result
class FlagsShift {
  bits<2> FlagN = 1;  // FLAG_MODIFIED
  bits<2> FlagZ = 1;  // FLAG_MODIFIED
  bits<2> FlagC = 1;  // FLAG_MODIFIED
}

// BIT instruction affects N, V, Z
class FlagsBIT {
  bits<2> FlagN = 1;  // FLAG_MODIFIED
  bits<2> FlagV = 1;  // FLAG_MODIFIED
  bits<2> FlagZ = 1;  // FLAG_MODIFIED
}

// SEC - Set Carry
class FlagsSetC { bits<2> FlagC = 2; }  // FLAG_SET

// CLC - Clear Carry
class FlagsClearC { bits<2> FlagC = 3; }  // FLAG_CLEARED

// SED - Set Decimal
class FlagsSetD { bits<2> FlagD = 2; }  // FLAG_SET

// CLD - Clear Decimal
class FlagsClearD { bits<2> FlagD = 3; }  // FLAG_CLEARED

// SEI - Set Interrupt Disable
class FlagsSetI { bits<2> FlagI = 2; }  // FLAG_SET

// CLI - Clear Interrupt Disable
class FlagsClearI { bits<2> FlagI = 3; }  // FLAG_CLEARED

// CLV - Clear Overflow
class FlagsClearV { bits<2> FlagV = 3; }  // FLAG_CLEARED

// PLP/RTI - All flags restored from stack
class FlagsAll {
  bits<2> FlagN = 1;
  bits<2> FlagV = 1;
  bits<2> FlagB = 1;
  bits<2> FlagD = 1;
  bits<2> FlagI = 1;
  bits<2> FlagZ = 1;
  bits<2> FlagC = 1;
}

//===----------------------------------------------------------------------===//
// Base Instruction Class
//===----------------------------------------------------------------------===//

/// The base MOS instruction class, from which all instructions descend.
class Inst<string asmstr> : Instruction {
  let Namespace = "MOS";

  // None of the MC instructions are considered to have any output operands.
  // Side-effects of the MC instructions
  let OutOperandList = (outs);

  let AsmString = asmstr;

  // Note: These properties are not tracked for the real instruction set, which
  // is used only in the MC layer. See the logical instruction set for the
  // definitions of these properties.
  let mayLoad = true;
  let mayStore = true;
  let hasSideEffects = true;

  let DecoderNamespace = "MOS";

  //=========================================================================
  // TSFlags Layout (30 bits used)
  //=========================================================================
  // Bits 0-3:   65816 M/X flag requirements
  // Bits 4-11:  Base cycle count (8 bits, 0-255)
  // Bits 12-15: Page cross penalty cycles (4 bits, 0-15)
  // Bits 16-29: Flag effects (2 bits each, 7 flags)
  //
  // Flag effect encoding: 0=unaffected, 1=modified, 2=set, 3=cleared

  // M/X flag handling for 65816 (bits 0-3)
  bit MLow = 0;
  bit MHigh = 0;
  bit XLow = 0;
  bit XHigh = 0;
  let TSFlags{0} = MLow;
  let TSFlags{1} = MHigh;
  let TSFlags{2} = XLow;
  let TSFlags{3} = XHigh;

  // Cycle counts (bits 4-15)
  // Base cycle count for NMOS 6502
  bits<8> Cycles6502 = 0;
  // Additional cycles when crossing page boundary (for indexed modes)
  bits<4> PageCrossCycles = 0;
  let TSFlags{11-4} = Cycles6502;
  let TSFlags{15-12} = PageCrossCycles;

  // Per-variant cycle overrides (not in TSFlags - for documentation/analysis only)
  // 0 = use Cycles6502, -1 = not available on this variant
  int Cycles65C02 = 0;
  int Cycles65816 = 0;
  int CyclesSPC700 = 0;

  // Flag effects (bits 16-29)
  // 2-bit encoding per flag: 0=unaffected, 1=modified, 2=set, 3=cleared
  bits<2> FlagN = 0;  // Negative
  bits<2> FlagV = 0;  // Overflow
  bits<2> FlagB = 0;  // Break (only affected by BRK/PHP/RTI)
  bits<2> FlagD = 0;  // Decimal
  bits<2> FlagI = 0;  // Interrupt disable
  bits<2> FlagZ = 0;  // Zero
  bits<2> FlagC = 0;  // Carry
  let TSFlags{17-16} = FlagN;
  let TSFlags{19-18} = FlagV;
  let TSFlags{21-20} = FlagB;
  let TSFlags{23-22} = FlagD;
  let TSFlags{25-24} = FlagI;
  let TSFlags{27-26} = FlagZ;
  let TSFlags{29-28} = FlagC;
}

//===----------------------------------------------------------------------===//
// Instruction Trait Classes
//===----------------------------------------------------------------------===//
// These classes add LLVM instruction properties (isBranch, isCall, etc.)
// to instructions. Prefix "Trait" distinguishes them from instruction classes.

class TraitBranch {
  bit isBranch = true;
  bit isTerminator = true;
}

class TraitUnconditionalBranch : TraitBranch {
  bit isBarrier = true;
}

class TraitCall {
  bit isCall = true;
}

class TraitReturn {
  bit isBarrier = true;
  bit isReturn = true;
  bit isTerminator = true;
}

/// Makes sure that immediate mode operands fit into the required bit field
/// size.  See also *isImm8 functions in MOS code.
class MOSAsmOperand<string name> : AsmOperandClass {
  let Name = name;
  let DiagnosticType = !strconcat("Invalid", name);
}

class ImmediateAsmOperand<string name> : MOSAsmOperand<name> {
    let RenderMethod = "addImmOperands";
    let DiagnosticType = "immediate";
  }

class PCRelativeOperand<string name> : MOSAsmOperand<name>;

/// An 8-bit (i.e. zero page) address.
class Addr8Operand<string name> : MOSAsmOperand<name>;

/// A 16-bit address.
class Addr16Operand<string name> : MOSAsmOperand<name>;

/// A 24-bit address.
class Addr24Operand<string name> : MOSAsmOperand<name>;

/// This operand will only match a value from 0 to 7 inclusive.
def imm3 : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm3">;
  let DecoderMethod = "decodeUImmOperand<3>";
  let OperandType = "OPERAND_IMM3";
  let OperandNamespace = "MOSOp";
  let Type = i8;
}

/// This operand will only match a value from 0 to 15 inclusive.
def imm4 : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm4">;
  let DecoderMethod = "decodeUImmOperand<4>";
  let OperandType = "OPERAND_IMM4";
  let OperandNamespace = "MOSOp";
  let Type = i8;
}

/// This operand will only match a value from 0 to 255 inclusive.
class imm8at<int offset> : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm8">;
  let EncoderMethod = "encodeImm<MOS::Imm8, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<8>";
  let OperandType = "OPERAND_IMM8";
  let OperandNamespace = "MOSOp";
  let Type = i8;
}
def imm8 : imm8at<1>;
def imm8at2 : imm8at<2>;

/// This operand will only match a value from 0 to 65535 inclusive.
class imm16at<int offset> : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm16">;
  let EncoderMethod = "encodeImm<MOS::Imm16, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<16>";
  let OperandType = "OPERAND_IMM16";
  let OperandNamespace = "MOSOp";
  let Type = i16;
}
def imm16 : imm16at<1>;
def imm16at5 : imm16at<5>;

/// An 8-bit pc-relative reference, for branch instructions.
class pcrel8at<int offset> : Operand<i32> {
  let ParserMatchClass = PCRelativeOperand<"PCRel8">;
  let EncoderMethod = "encodeImm<MOS::PCRel8, " # offset # ">";
  let DecoderMethod = "decodeSImmOperand<8>";
  let PrintMethod = "printBranchOperand";
  let OperandType = "OPERAND_PCREL";
}
def pcrel8 : pcrel8at<1>;
def pcrel8at2 : pcrel8at<2>;

/// A 16-bit pc-relative reference, for 65816 BRL instruction.
class pcrel16at<int offset> : Operand<i32> {
  let ParserMatchClass = PCRelativeOperand<"PCRel16">;
  let EncoderMethod = "encodeImm<MOS::PCRel16, " # offset # ">";
  let DecoderMethod = "decodeSImmOperand<16>";
  let PrintMethod = "printBranchOperand";
  let OperandType = "OPERAND_PCREL";
}
def pcrel16 : pcrel16at<1>;

class addr8at<int offset> : Operand<i32> {
  let ParserMatchClass = Addr8Operand<"Addr8">;
  let EncoderMethod = "encodeImm<MOS::Addr8, " # offset # ">";
  let DecoderMethod = "decodeAddr8Operand";
  let OperandType = "OPERAND_ADDR8";
  let OperandNamespace = "MOSOp";
  let Type = i8;
}
def addr8 : addr8at<1>;
def addr8at2 : addr8at<2>;
def addr8at3 : addr8at<3>;
def addr8at4 : addr8at<4>;

class addr16at<int offset> : Operand<i32> {
  let ParserMatchClass = Addr16Operand<"Addr16">;
  let EncoderMethod = "encodeImm<MOS::Addr16, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<16>";
  let OperandType = "OPERAND_ADDR16";
  let OperandNamespace = "MOSOp";
  let Type = i16;
}
def addr16 : addr16at<1>;
def addr16at2 : addr16at<2>;
def addr16at3 : addr16at<3>;

class addr24at<int offset> : Operand<i32> {
  let ParserMatchClass = Addr24Operand<"Addr24">;
  let EncoderMethod = "encodeImm<MOS::Addr24, " # offset # ">";
  let DecoderMethod = "decodeUImmOperand<24>";
  let OperandType = "OPERAND_ADDR24";
  let OperandNamespace = "MOSOp";
}
def addr24 : addr24at<1>;

/// All non-virtual MOS opcodes are 8 bits long.
class Opcode<bits<8> opcode = 0> {
  bits<8> op = opcode;
}
def DefaultOpcode : Opcode<0>;

/// Per Parker, opcodes on the MOS series tend to fit into the pattern aaabbbcc
/// where bbb is the addressing mode, and aaacc describe the instruction in
/// question. This is not a fixed rule however, especially for later processors
/// in the MOS series.
/// Parker: "Most instructions that explicitly reference memory locations have
/// bit patterns of the form aaabbbcc. The aaa and cc bits determine the
/// opcode, and the bbb bits determine the addressing mode."
/// Note: This class is kept for SPC700 compatibility. For 6502 instructions,
/// use explicit Opcode<0xNN> values for clarity.
class OpcodeABC<bits<3> aaa = 0, bits<3> bbb = 0, bits<2> cc = 0> : Opcode<0> {
  let op{7-5} = aaa;
  let op{4-2} = bbb;
  let op{1-0} = cc;
}

/// Addressing modes
/// ----------------
/// Addressing modes on 65xx do not perfectly map to bit fields across all
/// instructions. So we define the concepts of the addressing modes here, and
/// define how the operands should be parsed per addressing mode.
/// I wrote the operands in the a i r y format because tablegen seems to like
/// to collapse multiple characters into one token if they are not separated
/// by spaces.  We'll remove those spaces when we print out the operands for
/// any instruction. See also MOSInstPrinter.cpp.
class AddressingMode<int size> {
  string OperandsStr = ?;
  dag InOperandList = ?;
  int OperandSize = size;  // Size of operand in bytes (instruction size = 1 + this)
}

def Implicit : AddressingMode<0> {
  let OperandsStr = "";
  let InOperandList = (ins);
}

def Accumulator : AddressingMode<0> {
  let OperandsStr = "";
  let InOperandList = (ins);
}

def Immediate : AddressingMode<1> {
  let OperandsStr = "#$param";
  let InOperandList = (ins imm8:$param);
}

def Immediate16 : AddressingMode<2> {
  let OperandsStr = "#$param";
  let InOperandList = (ins imm16:$param);
}

def ZeroPage : AddressingMode<1> {
  let OperandsStr = "$param";
  let InOperandList = (ins addr8:$param);
}

def ZeroPageX : AddressingMode<1> {
  let OperandsStr = "$param , x";
  let InOperandList = (ins addr8:$param);
}

def ZeroPageY : AddressingMode<1> {
  let OperandsStr = "$param , y";
  let InOperandList = (ins addr8:$param);
}

def Relative : AddressingMode<1> {
  let OperandsStr = "$param";
  let InOperandList = (ins pcrel8:$param);
}

def Relative16 : AddressingMode<2> {
  let OperandsStr = "$param";
  let InOperandList = (ins pcrel16:$param);
}

def Absolute : AddressingMode<2> {
  let OperandsStr = "$param";
  let InOperandList = (ins addr16:$param);
}

def AbsoluteLong : AddressingMode<3> {
  let OperandsStr = "$param";
  let InOperandList = (ins addr24:$param);
}

def AbsoluteX : AddressingMode<2> {
  let OperandsStr = "$param , x";
  let InOperandList = (ins addr16:$param);
}

def AbsoluteXLong : AddressingMode<3> {
  let OperandsStr = "$param , x";
  let InOperandList = (ins addr24:$param);
}

def AbsoluteY : AddressingMode<2> {
  let OperandsStr = "$param , y";
  let InOperandList = (ins addr16:$param);
}

def Indirect : AddressingMode<1> {
  let OperandsStr = "( $param )";
  let InOperandList = (ins addr8:$param);
}

def Indirect16 : AddressingMode<2> {
  let OperandsStr = "( $param )";
  let InOperandList = (ins addr16:$param);
}

def IndexedIndirect16 : AddressingMode<2> {
  let OperandsStr = "( $param , x )";
  let InOperandList = (ins addr16:$param);
}

def IndirectLong : AddressingMode<1> {
  let OperandsStr = "[ $param ]";
  let InOperandList = (ins addr8:$param);
}

def IndexedIndirect : AddressingMode<1> {
  let OperandsStr = "( $param , x )";
  let InOperandList = (ins addr8:$param);
}

def IndirectIndexed : AddressingMode<1> {
  let OperandsStr = "( $param ) , y";
  let InOperandList = (ins addr8:$param);
}

def IndirectIndexedLong : AddressingMode<1> {
  let OperandsStr = "[ $param ] , y";
  let InOperandList = (ins addr8:$param);
}

def IndirectIndexedLongZ : AddressingMode<1> {
  let OperandsStr = "[ $param ] , z";
  let InOperandList = (ins addr8:$param);
}

def IndirectZeroPageZ : AddressingMode<1> {
  let OperandsStr = "( $param ) , z";
  let InOperandList = (ins addr8:$param);
}

def StackRelative : AddressingMode<1> {
  let OperandsStr = "$param , s";
  let InOperandList = (ins addr8:$param);
}

def IndirectStackRelativeY : AddressingMode<1> {
  let OperandsStr = "( $param , s ) , y";
  let InOperandList = (ins addr8:$param);
}

def RStackRelative : AddressingMode<1> {
  let OperandsStr = "$param , r";
  let InOperandList = (ins addr8:$param);
}

def IndirectRStackRelativeY : AddressingMode<1> {
  let OperandsStr = "( $param , r ) , y";
  let InOperandList = (ins addr8:$param);
}

def ZeroPageAt2 : AddressingMode<1> {
  let OperandsStr = "$param";
  let InOperandList = (ins addr8at2:$param);
}

def ZeroPageXAt2 : AddressingMode<1> {
  let OperandsStr = "$param , x";
  let InOperandList = (ins addr8at2:$param);
}

def AbsoluteAt2 : AddressingMode<2> {
  let OperandsStr = "$param";
  let InOperandList = (ins addr16at2:$param);
}

def AbsoluteXAt2 : AddressingMode<2> {
  let OperandsStr = "$param , x";
  let InOperandList = (ins addr16at2:$param);
}

/// An unsized MOS instruction, to which an addressing mode may be applied.
class InstAddressMode<string asmstr, Opcode op = DefaultOpcode,
                      AddressingMode mode = Implicit> : Inst<asmstr> {
  Opcode opcode = op;
  let InOperandList = mode.InOperandList;
  string OperandsStr;
  let OperandsStr = mode.OperandsStr;
}

/// An 8 bit real MOS instruction.
class Inst8<string asmstr, Opcode op = DefaultOpcode,
            AddressingMode mode = Implicit> :
    InstAddressMode<asmstr, op, mode> {
  let Size = 1;
  bits<8> Inst;
  let Inst{7-0} = opcode.op;
}

/// A 16 bit real MOS instruction.  Always an 8 bit opcode and an 8 bit
/// operand.
class Inst16<string opcodestr, Opcode op = DefaultOpcode,
             AddressingMode mode = Implicit> :
    InstAddressMode<opcodestr, op, mode> {
  let Size = 2;
  bits<8> param;
  bits<16> Inst;
  let Inst{7-0} = opcode.op;
  let Inst{15-8} = param;
  let AsmString = opcodestr # " " # mode.OperandsStr;
}

/// A 24 bit real MOS instruction. Always an 8 bit opcode and a 16 bit
/// operand.
class Inst24<string opcodestr, Opcode op = DefaultOpcode,
             AddressingMode mode = Implicit> :
    InstAddressMode<opcodestr, op, mode> {
  let Size = 3;
  bits<16> param;
  bits<24> Inst;
  let Inst{7-0} = opcode.op;
  let Inst{23-8} = param;
  let AsmString = opcodestr # " " # mode.OperandsStr;
}

/// A 32 bit real 65816 instruction. Always an 8 bit opcode and a 24 bit
/// operand.
class Inst32<string opcodestr, Opcode op = DefaultOpcode,
             AddressingMode mode = Implicit> :
    InstAddressMode<opcodestr, op, mode> {
  let Size = 4;
  bits<24> param;
  bits<32> Inst;
  let Inst{7-0} = opcode.op;
  let Inst{31-8} = param;
  let AsmString = opcodestr # " " # mode.OperandsStr;
}

//===----------------------------------------------------------------------===//
// Data-Driven Instruction Definition
//===----------------------------------------------------------------------===//
//
// MOSInst - Unified instruction class that derives size from addressing mode.
// The OperandSize field in AddressingMode determines instruction encoding:
//   OperandSize=0 -> 1-byte instruction (opcode only)
//   OperandSize=1 -> 2-byte instruction (opcode + 8-bit operand)
//   OperandSize=2 -> 3-byte instruction (opcode + 16-bit operand)
//   OperandSize=3 -> 4-byte instruction (opcode + 24-bit operand)
//
// Example:
//   defm LDA : Op<0xA5, "lda", ZeroPage>;   // creates LDA_ZeroPage (2 bytes)
//   defm BRK : Op<0x00, "brk", Implicit>;   // creates BRK_Implicit (1 byte)
//

class MOSInst<bits<8> opcode, string mnemonic, AddressingMode mode>
    : Inst<!if(!eq(mode.OperandSize, 0), mnemonic, mnemonic # " " # mode.OperandsStr)> {
  let Size = !add(1, mode.OperandSize);
  let InOperandList = mode.InOperandList;

  // Use 32-bit field to accommodate all sizes; unused bits stay zero
  bits<24> param;
  bits<32> Inst;
  let Inst{7-0} = opcode;
  // Operand bits: conditional assignment based on size
  let Inst{15-8}  = !if(!ge(mode.OperandSize, 1), param{7-0}, 0);
  let Inst{23-16} = !if(!ge(mode.OperandSize, 2), param{15-8}, 0);
  let Inst{31-24} = !if(!ge(mode.OperandSize, 3), param{23-16}, 0);
}

/// Op - Generate instruction with standard naming: MNEMONIC_AddressingMode
/// Size is automatically derived from the addressing mode's OperandSize.
/// Note: "Implicit" mode generates "_Implied" suffix per 6502 Programming Manual.
multiclass Op<bits<8> opcode, string mnemonic, AddressingMode mode> {
  defvar suffix = !if(!eq(!cast<string>(mode), "Implicit"), "Implied", !cast<string>(mode));
  def NAME # _ # suffix : MOSInst<opcode, mnemonic, mode>;
}

//===----------------------------------------------------------------------===//
// Specialized Multiclasses for Control Flow Instructions
//===----------------------------------------------------------------------===//

/// OpBranch - Generate conditional branch instruction (8-bit relative addressing).
/// Creates: MNEMONIC_Relative
multiclass OpBranch<bits<8> opcode, string mnemonic> {
  def NAME # _Relative : MOSInst<opcode, mnemonic, Relative>, TraitBranch;
}

/// OpBranch16 - Generate conditional branch instruction (16-bit relative addressing).
/// Creates: MNEMONIC_Relative16
multiclass OpBranch16<bits<8> opcode, string mnemonic> {
  def NAME # _Relative16 : MOSInst<opcode, mnemonic, Relative16>, TraitBranch;
}

/// OpJump - Generate unconditional branch instruction.
/// Creates: MNEMONIC_AddressingMode with TraitUnconditionalBranch
multiclass OpJump<bits<8> opcode, string mnemonic, AddressingMode mode> {
  defvar suffix = !cast<string>(mode);
  def NAME # _ # suffix : MOSInst<opcode, mnemonic, mode>, TraitUnconditionalBranch;
}

/// OpJumpIndirect - Generate indirect unconditional branch instruction.
/// Creates: MNEMONIC_AddressingMode with TraitUnconditionalBranch and isIndirectBranch
multiclass OpJumpIndirect<bits<8> opcode, string mnemonic, AddressingMode mode> {
  defvar suffix = !cast<string>(mode);
  def NAME # _ # suffix : MOSInst<opcode, mnemonic, mode>, TraitUnconditionalBranch {
    let isIndirectBranch = true;
  }
}

/// OpCallIndirect - Generate indirect call instruction.
/// Creates: MNEMONIC_AddressingMode with TraitCall and isIndirectBranch
multiclass OpCallIndirect<bits<8> opcode, string mnemonic, AddressingMode mode> {
  defvar suffix = !cast<string>(mode);
  def NAME # _ # suffix : MOSInst<opcode, mnemonic, mode>, TraitCall {
    let isIndirectBranch = true;
  }
}

/// OpCall - Generate call instruction.
/// Creates: MNEMONIC_AddressingMode with TraitCall
multiclass OpCall<bits<8> opcode, string mnemonic, AddressingMode mode> {
  defvar suffix = !cast<string>(mode);
  def NAME # _ # suffix : MOSInst<opcode, mnemonic, mode>, TraitCall;
}

/// OpReturn - Generate return instruction.
/// Creates: MNEMONIC_Implied with TraitReturn
multiclass OpReturn<bits<8> opcode, string mnemonic> {
  def NAME # _Implied : MOSInst<opcode, mnemonic, Implicit>, TraitReturn;
}

/// OpReturnWithMode - Generate return instruction with specific addressing mode.
/// Creates: MNEMONIC_AddressingMode with TraitReturn
multiclass OpReturnWithMode<bits<8> opcode, string mnemonic, AddressingMode mode> {
  defvar suffix = !cast<string>(mode);
  def NAME # _ # suffix : MOSInst<opcode, mnemonic, mode>, TraitReturn;
}

//===----------------------------------------------------------------------===//
// Specialized Addressing Modes
//===----------------------------------------------------------------------===//

/// BitBranch - R65C02 BBR/BBS instructions: 3 bytes with bit index, address, branch target
def BitBranch : AddressingMode<2> {
  let OperandsStr = "$tbit , $source , $location";
  let InOperandList = (ins imm3:$tbit, addr8:$source, pcrel8at2:$location);
}

/// BitModify - R65C02 RMB/SMB instructions: 2 bytes with bit index and address
def BitModify : AddressingMode<1> {
  let OperandsStr = "$tbit , $param";
  let InOperandList = (ins imm3:$tbit, addr8:$param);
}

/// MemoryMove - 65816 MVN/MVP instructions: 3 bytes with two bank addresses
def MemoryMove : AddressingMode<2> {
  let OperandsStr = "#$sourceBank , #$destinationBank";
  let InOperandList = (ins imm8:$sourceBank, imm8at2:$destinationBank);
}

/// BlockMove - HuC6280 TAI/TIA/etc: 7 bytes with source, dest, length
def BlockMove : AddressingMode<6> {
  let OperandsStr = "$bsource , $bdest , #$blength";
  let InOperandList = (ins addr16:$bsource, addr16at3:$bdest, imm16at5:$blength);
}

/// ImmediateZeroPage - HuC6280 TST: immediate + zero page address
def ImmediateZeroPage : AddressingMode<2> {
  let OperandsStr = "#$imm , $param";
  let InOperandList = (ins imm8:$imm, addr8at2:$param);
}

/// ImmediateAbsolute - HuC6280 TST: immediate + absolute address
def ImmediateAbsolute : AddressingMode<3> {
  let OperandsStr = "#$imm , $param";
  let InOperandList = (ins imm8:$imm, addr16at2:$param);
}

//===----------------------------------------------------------------------===//
// Specialized Multiclasses for Non-Standard Instructions
//===----------------------------------------------------------------------===//

/// OpBitBranch - R65C02 BBR/BBS bit test and branch
/// Opcode has variable bits: bit 7 determines BBR(0) vs BBS(1), bits 6-4 are the bit number
multiclass OpBitBranch<bits<8> opcode, string mnemonic> {
  def NAME # _BitBranch : MOSInst<opcode, mnemonic, BitBranch>, TraitBranch {
    bits<3> tbit;
    // Override the instruction encoding for the variable bit field
    let Inst{7} = opcode{7};
    let Inst{6-4} = tbit;
    let Inst{3-0} = opcode{3-0};
  }
}

/// OpBitModify - R65C02 RMB/SMB bit reset/set
/// Opcode has variable bits: bit 7 determines RMB(0) vs SMB(1), bits 6-4 are the bit number
multiclass OpBitModify<bits<8> opcode, string mnemonic> {
  def NAME # _BitModify : MOSInst<opcode, mnemonic, BitModify> {
    bits<3> tbit;
    // Override the instruction encoding for the variable bit field
    let Inst{7} = opcode{7};
    let Inst{6-4} = tbit;
    let Inst{3-0} = opcode{3-0};
  }
}

/// OpMemoryMove - 65816 MVN/MVP block memory move
multiclass OpMemoryMove<bits<8> opcode, string mnemonic> {
  def NAME # _MemoryMove : MOSInst<opcode, mnemonic, MemoryMove>;
}

/// OpBlockMove - HuC6280 TAI/TIA/TDD/TIN/TII block transfer
multiclass OpBlockMove<bits<8> opcode, string mnemonic> {
  def NAME # _BlockMove : MOSInst<opcode, mnemonic, BlockMove>;
}

//===----------------------------------------------------------------------===//

/// A 24 bit immediate prefixed HuC6280 instruction.
/// Always an 8 bit opcode, an 8 bit immediate and an 8 bit operand.
class InstImmediate24<string opcodestr, Opcode op = DefaultOpcode,
             AddressingMode mode = Implicit> :
    InstAddressMode<opcodestr, op, mode> {
  let Size = 3;
  bits<8> imm;
  bits<8> param;
  bits<24> Inst;

  Opcode opcode = op;
  let InOperandList = !con((ins imm8:$imm), mode.InOperandList);
  string OperandsStr;
  let OperandsStr = "#$imm , " # mode.OperandsStr;

  let Inst{7-0} = opcode.op;
  let Inst{15-8} = imm;
  let Inst{23-16} = param;
  let AsmString = opcodestr # " " # OperandsStr;
}

/// A 32 bit immediate prefixed HuC6280 instruction.
/// Always an 8 bit opcode, an 8 bit immediate and a 16 bit operand.
class InstImmediate32<string opcodestr, Opcode op = DefaultOpcode,
             AddressingMode mode = Implicit> :
    InstAddressMode<opcodestr, op, mode> {
  let Size = 4;
  bits<8> imm;
  bits<16> param;
  bits<32> Inst;

  Opcode opcode = op;
  let InOperandList = !con((ins imm8:$imm), mode.InOperandList);
  string OperandsStr;
  let OperandsStr = "#$imm , " # mode.OperandsStr;

  let Inst{7-0} = opcode.op;
  let Inst{15-8} = imm;
  let Inst{31-16} = param;
  let AsmString = opcodestr # " " # OperandsStr;
}

/// A R65C02 bit branch instruction.
class InstBitBranch<string opcodestr, Opcode op = DefaultOpcode> :
    Inst<opcodestr>, TraitBranch {
  let Size = 3;
  bits<24> Inst;
  bits<3> tbit;
  bits<8> source;
  bits<8> location;

  Opcode opcode = op;
  let InOperandList = (ins imm3:$tbit, addr8:$source, pcrel8at2:$location);
  string OperandsStr;
  let OperandsStr = "$tbit , $source , $location";

  let Inst{7} = opcode.op{7};
  let Inst{6-4} = tbit;
  let Inst{3-0} = opcode.op{3-0};
  let Inst{15-8} = source;
  let Inst{23-16} = location;
  let AsmString = opcodestr # " " # OperandsStr;
}

/// A R65C02 bit modify instruction.
class InstBitModify<string opcodestr, Opcode op = DefaultOpcode,
                    AddressingMode mode = Implicit> :
    InstAddressMode<opcodestr, op, mode> {
  let Size = 2;
  bits<8> param;
  bits<16> Inst;
  bits<3> tbit;

  Opcode opcode = op;
  let InOperandList = !con((ins imm3:$tbit), mode.InOperandList);
  string OperandsStr;
  let OperandsStr = "$tbit , " # mode.OperandsStr;

  let Inst{7} = opcode.op{7};
  let Inst{6-4} = tbit;
  let Inst{3-0} = opcode.op{3-0};
  let Inst{15-8} = param;
  let AsmString = opcodestr # " " # OperandsStr;
}

/// 45GS02: NOP prefixed opcode with 8-bit address operand (flat memory access)
class Inst4502Nop16<string opcodestr, Opcode op = DefaultOpcode, AddressingMode mode = IndirectIndexedLongZ> :
    Inst<opcodestr> {
  let Size = 3;
  bits<24> Inst;
  bits<8> param;

  Opcode opcode = op;
  let InOperandList = (ins addr8at2:$param);

  let Inst{7-0} = 0xea; // NOP
  let Inst{15-8} = opcode.op;
  let Inst{23-16} = param;
  let AsmString = opcodestr # " " # mode.OperandsStr;
}

/// 45GS02: NEG NEG prefixed opcode with 8-bit address operand
class Inst4502NegNeg8<string opcodestr, Opcode op = DefaultOpcode, AddressingMode mode = ZeroPageX> :
    Inst<opcodestr> {
  let Size = 4;
  bits<32> Inst;
  bits<8> param;

  Opcode opcode = op;
  let InOperandList = (ins addr8at3:$param);
  let Inst{7-0} = 0x42;  // NEG
  let Inst{15-8} = 0x42; // NEG
  let Inst{23-16} = opcode.op;
  let Inst{31-24} = param;
  let AsmString = opcodestr # " " # mode.OperandsStr;
}

/// 45GS02: NEG NEG prefixed opcode; no operand
class Inst4502NegNegImplicit<string opcodestr, Opcode op = DefaultOpcode, AddressingMode mode = Implicit> :
    Inst<opcodestr> {
  let Size = 3;
  bits<24> Inst;
  let InOperandList = mode.InOperandList;

  Opcode opcode = op;
  let Inst{7-0} = 0x42;  // NEG
  let Inst{15-8} = 0x42; // NEG
  let Inst{23-16} = opcode.op;
  let AsmString = opcodestr;
}

/// 45GS02: NEG NEG prefixed opcode with 16-bit address operand
class Inst4502NegNeg16<string opcodestr, Opcode op = DefaultOpcode, AddressingMode mode = AbsoluteX> :
    Inst<opcodestr> {
  let Size = 5;
  bits<40> Inst;
  bits<16> param;

  Opcode opcode = op;
  let InOperandList = (ins addr16at3:$param);

  let Inst{7-0} = 0x42;  // NEG
  let Inst{15-8} = 0x42; // NEG
  let Inst{23-16} = opcode.op;
  let Inst{39-24} = param;
  let AsmString = opcodestr # " " # mode.OperandsStr;
}

/// 45GS02: NEG NEG NOP prefixed opcode with 8-bit address operand
class Inst4502NegNegNop8<string opcodestr, Opcode op = DefaultOpcode, AddressingMode mode = IndirectLong> :
    Inst<opcodestr> {
  let Size = 5;
  bits<40> Inst;
  bits<8> param;

  Opcode opcode = op;
  let InOperandList = (ins addr8at4:$param);

  let Inst{7-0} = 0x42;  // NEG
  let Inst{15-8} = 0x42; // NEG
  let Inst{23-16} = 0xea;// NOP
  let Inst{31-24} = opcode.op;
  let Inst{39-32} = param;
  let AsmString = opcodestr # " " # mode.OperandsStr;
}

/// A HuC6280 block instruction.
class InstHuCBlockMove<string opcodestr, Opcode op = DefaultOpcode> :
    Inst<opcodestr> {
  let Size = 7;
  bits<56> Inst;
  bits<16> bsource;
  bits<16> bdest;
  bits<16> blength;

  Opcode opcode = op;
  let InOperandList = (ins addr16:$bsource, addr16at3:$bdest, imm16at5:$blength);
  string OperandsStr;
  let OperandsStr = "$bsource , $bdest , #$blength";

  let Inst{7-0} = opcode.op;
  let Inst{23-8} = bsource;
  let Inst{39-24} = bdest;
  let Inst{55-40} = blength;
  let AsmString = opcodestr # " " # OperandsStr;
}

/// A 65816 memory move instruction.
class Inst816MemoryMove<string opcodestr, Opcode op = DefaultOpcode> :
    Inst<opcodestr> {
  let Size = 3;
  bits<24> Inst;
  bits<8> sourceBank;
  bits<8> destinationBank;

  Opcode opcode = op;
  let InOperandList = (ins imm8:$sourceBank, imm8at2:$destinationBank);
  string OperandsStr;
  let OperandsStr = "#$sourceBank , #$destinationBank";

  let Inst{7-0} = opcode.op;
  let Inst{15-8} = sourceBank;
  let Inst{23-16} = destinationBank;
  let AsmString = opcodestr # " " # OperandsStr;
}

/// Some one-byte opcodes tend to have fixed lower 4 bits, and vary in the
/// higher four bits.
class InstNybble<string asmstr, bits<4> low = 0, bits<4> high = 0> :
    Inst8<asmstr> {
  let Inst{7-4} = high;
  let Inst{3-0} = low;
}

/// Instructions where the low nybble is 0x0.
class InstLow0<string asmstr, bits<4> high = 0> : InstNybble<asmstr, 0x0, high>;
/// Instructions where the low nybble is 0x2.
class InstLow2<string asmstr, bits<4> high = 0> : InstNybble<asmstr, 0x2, high>;
/// Instructions where the low nybble is 0x3.
class InstLow3<string asmstr, bits<4> high = 0> : InstNybble<asmstr, 0x3, high>;
/// Instructions where the low nybble is 0x4.
class InstLow4<string asmstr, bits<4> high = 0> : InstNybble<asmstr, 0x4, high>;
/// Instructions where the low nybble is 0x8.
class InstLow8<string asmstr, bits<4> high = 0> : InstNybble<asmstr, 0x8, high>;
/// Instructions where the low nybble is 0xa.
class InstLowA<string asmstr, bits<4> high = 0> : InstNybble<asmstr, 0xa, high>;
/// Instructions where the low nybble is 0xb.
class InstLowB<string asmstr, bits<4> high = 0> : InstNybble<asmstr, 0xb, high>;


/// "The conditional branch instructions all have the form xxy10000. The flag
/// indicated by xx is compared with y, and the branch is taken if they are
/// equal.
/// xx flag
/// 00 negative
/// 01 overflow
/// 10 carry
/// 11 zero
/// This gives the following branches:
/// BPL BMI BVC BVS BCC BCS BNE BEQ
/// 10  30  50  70  90  B0  D0  F0

// Note: Parameter order is <opcode, mnemonic> to match MOSInst convention.
class ConditionalBranch<bits<8> opcode, string mnemonic> :
    Inst16<mnemonic, Opcode<opcode>, Relative>, TraitBranch;
