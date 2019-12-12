//===-- MOSInstrInfo.td - MOS Instruction Formats ----------*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// MOS Instruction Format Definitions.
//
//===----------------------------------------------------------------------===//

/* Much of the structure of this file is dependent on Neil Parker's analysis of
 * the 65xx instruction set, at http://nparker.llx.com/a2/opcodes.html .
 * Parker's analysis is closely reflected in the structure of this file.
 */

// The base MOS instruction class, from which all instructions descend.
class Inst< dag outs, dag ins, string asmstr, list<dag> pattern> : Instruction
{
  let Namespace = "MOS";

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;

  field bits<32> SoftFail = 0;
  let DecoderNamespace = "MOS";
}

/*
 * Helps to make sure immediate mode operands fit into the requisite bit field size.
 * See also *isImm8 functions in code.
 */
class ImmediateAsmOperand<string name>
  : AsmOperandClass {
  let Name = name;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = !strconcat("Invalid", name);
}

/* This operand will only match an 8 bit value. */
def imm8 : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm8">;
}

/* This operand will only match a 16 bit value. */
def imm16 : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm16">;
}

/* This operand will only match a value between 8 and 16 bits
 * (e.g. between 256 and 65535 inclusive). */
def imm8to16 : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm8To16">;
}

/* All MOS opcodes are 8 bits long. */
class Opcode< bits<8> opcode = 0 >
{
  bits< 8 > op;
  let op = opcode;
}
def DefaultOpcode : Opcode< 0 >;

/* Per Parker, opcodes on the MOS series tend to fit into the pattern aaabbbcc, where
 * bbb is the addressing mode, and aaacc describe the instruction in question. 
 * This is not a fixed rule however, especially for later processors in the
 * MOS series.
 * 
 * Parker: "Most instructions that explicitly reference memory locations have bit
 * patterns of the form aaabbbcc. The aaa and cc bits determine the opcode, and the bbb 
 * bits determine the addressing mode."
 */
class OpcodeABC< bits<3> aaa = 0, bits<3> bbb = 0, bits<2> cc = 0 > : Opcode< 0 >
{
  let op{7-5} = aaa;
  let op{4-2} = bbb;
  let op{1-0} = cc;
}

/* Instructions tend to break down into similar addressing modes by the cc 
 * bits. So we deal with base instruction classes in four groups.
 */
class OpcodeC0< bits<3> aaa = 0, bits<3> bbb = 0 > : OpcodeABC< aaa, bbb, 0b00 >;
class OpcodeC1< bits<3> aaa = 0, bits<3> bbb = 0 > : OpcodeABC< aaa, bbb, 0b01 >;
class OpcodeC2< bits<3> aaa = 0, bits<3> bbb = 0 > : OpcodeABC< aaa, bbb, 0b10 >;
class OpcodeC3< bits<3> aaa = 0, bits<3> bbb = 0 > : OpcodeABC< aaa, bbb, 0b11 >;

/* Addressing modes on 65xx do not perfectly map to bit fields across all instructions.
 * So we define the concepts of the addressing modes here, and define how the operands
 * should be parsed per addressing mode.
 */
class AddressingMode
{
  string OperandsStr = "";
  dag InOperandList = (ins);
}

def Implicit : AddressingMode
{
  string OperandsStr = "";
  dag InOperandList = (ins);
}

def Immediate : AddressingMode
{
  let OperandsStr = "#$param";
  let InOperandList = (ins imm8:$param);
}

def ZeroPage : AddressingMode
{
  let OperandsStr = "$param";
  let InOperandList = (ins imm8:$param);
}

def ZeroPageX : AddressingMode
{
  let OperandsStr = "$param,x";
  let InOperandList = (ins imm8:$param);
}

def ZeroPageY : AddressingMode
{
  let OperandsStr = "$param,y";
  let InOperandList = (ins imm8:$param);
}

def Relative : AddressingMode
{
  let OperandsStr = "$param";
  let InOperandList = (ins imm8:$param);
}

def Absolute : AddressingMode
{
  let OperandsStr = "$param";
  let InOperandList = (ins imm8to16:$param);
}

def AbsoluteX : AddressingMode
{
  let OperandsStr = "$param,x";
  let InOperandList = (ins imm8to16:$param);
}

def AbsoluteY : AddressingMode
{
  let OperandsStr = "$param,y";
  let InOperandList = (ins imm8to16:$param);
}

def Indirect : AddressingMode
{
  let OperandsStr = "($param)";
  let InOperandList = (ins imm8to16:$param);
}

def IndexedIndirect : AddressingMode
{
  let OperandsStr = "($param,x)";
  let InOperandList = (ins imm8:$param);
}

def IndirectIndexed : AddressingMode
{
  let OperandsStr = "( $param ),y";
  let InOperandList = (ins imm8:$param);
}

/* An unsized MOS instruction, to which an addressing mode may be applied. */
class InstAddressMode< dag outs, dag ins, string asmstr, list<dag> pattern, 
             Opcode op = DefaultOpcode, AddressingMode mode = Implicit > :
    Inst< outs, ins, asmstr, pattern >
{
  let InOperandList = mode.InOperandList;
  string OperandsStr;
  let OperandsStr = mode.OperandsStr;
}

/* An 8 bit MOS instruction. */
class Inst8< dag outs, dag ins, string asmstr, list<dag> pattern, 
             Opcode op = DefaultOpcode, AddressingMode mode = Implicit > :
  InstAddressMode< outs, ins, asmstr, pattern, op, mode>
{
  let Size = 1;

  Opcode opcode;
  let opcode = op;

  bits<8> Inst;
  let Inst{7-0} = opcode.op;
}

/* A 16 bit MOS instruction.  Always an 8 bit opcode and an 8 bit parameter. */
class Inst16< dag outs, dag ins, string opcodestr = "unknown", list<dag> pattern, 
              Opcode op = DefaultOpcode, AddressingMode mode = Implicit> :
InstAddressMode< outs, ins, opcodestr, pattern, op, mode>
{
  let Size = 2;

  Opcode opcode;
  let opcode = op;

  bits<8> param;
    
  bits<16> Inst;
  let Inst{7-0} = opcode.op;
  let Inst{15-8} = param;

  let AsmString = opcodestr # " " # mode.OperandsStr;
}

/* A 24 bit MOS instruction. Always an 8 bit opcode and a 16 bit parameter. */
class Inst24< dag outs, dag ins, string opcodestr = "unknown", list<dag> pattern,
              Opcode op = DefaultOpcode, AddressingMode mode = Implicit> :
InstAddressMode< outs, ins, opcodestr, pattern, op, mode>
{
  let Size = 3;

  Opcode opcode;
  let opcode = op;

  bits<16> param;
  
  bits<24> Inst;
  let Inst{7-0} = opcode.op;
  let Inst{23-8} = param;

  let AsmString = opcodestr # " " # mode.OperandsStr;
}


/* OpcodeC1 class instructions */
/* Because the cc = 01 instructions are relatively regular, we sketch them out first. */
multiclass CC1_NoImmediate< bits<3> aaa, dag outs, dag ins, string OpcodeStr = "nop", 
    list<dag> pattern = []> 
{
/*
For cc = 01, then bbb equals:
000	(zero page,X)
001	zero page
010	#immediate
011	absolute
100	(zero page),Y
101	zero page,X
110	absolute,Y
111	absolute,X
*/
  def _IndexedIndirect : 
    Inst16< outs, ins, OpcodeStr, [], OpcodeC1< aaa, 0b000 >, IndexedIndirect >;

  def _ZeroPage : 
    Inst16< outs, ins, OpcodeStr, [], OpcodeC1< aaa, 0b001 >, ZeroPage >;

  /* skip immediate instruction types here for 0b010 */
  
  def _Absolute : 
    Inst24< outs, ins, OpcodeStr, [], OpcodeC1< aaa, 0b011 >, Absolute >;

  def _IndirectIndexed : 
    Inst16< outs, ins, OpcodeStr, [], OpcodeC1< aaa, 0b100 >, IndirectIndexed >;

  def _ZeroPageX :
    Inst16< outs, ins, OpcodeStr, [], OpcodeC1< aaa, 0b101 >, ZeroPageX >;

  def _AbsoluteY : 
    Inst24< outs, ins, OpcodeStr, [], OpcodeC1< aaa, 0b110 >, AbsoluteY >;

  def _AbsoluteX : 
    Inst24< outs, ins, OpcodeStr, [], OpcodeC1< aaa, 0b111 >, AbsoluteX >;
}

/* All cc=01 instruction types, for the majority of opcodes where cc-01 */
multiclass CC1_All< bits<3> aaa, dag outs, dag ins, string OpcodeStr = "nop", list<dag> pattern = []> :
  CC1_NoImmediate< aaa, outs, ins, OpcodeStr, pattern> 
{
    // handle immediate CC1 instruction types here for opcodes that fit the CC1 pattern
    def _Immediate : 
      Inst16< outs, ins, OpcodeStr, [], OpcodeC1< aaa, 0b010 >, Immediate >;
}

/* "The conditional branch instructions all have the form xxy10000. The flag indicated by
 * xx is compared with y, and the branch is taken if they are equal."
xx	flag
00	negative
01	overflow
10	carry
11	zero
This gives the following branches:

BPL	BMI	BVC	BVS	BCC	BCS	BNE	BEQ
10	30	50	70	90	B0	D0	F0
*/

class ConditionalBranch< string opcodestr = "unknown", dag outs, dag ins, list<dag> pattern, bits<2> flagType, 
    bits<1> value >:
  Inst16< outs, ins, opcodestr, pattern, DefaultOpcode, Relative>
{
   bits<3> a;
   let a{2-1} = flagType;
   let a{0} = value;
   let opcode = OpcodeABC< a, 0b100, 0b00 >;
}
  

/* Predicates.  Useful for limiting instructions to particular hardware modes or 
 * particular hardware implementations.
 */
def Has6502 : Predicate<"Subtarget->has6502()">,
                         AssemblerPredicate<"Feature6502">
{
  let PredicateName = "Feature6502";
}


