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

class ImmediateAsmOperand<string name>
  : AsmOperandClass {
  let Name = name;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = !strconcat("Invalid", name);
}

def imm8 : Operand<i32> {
  let ParserMatchClass = ImmediateAsmOperand<"Imm8">;
}


class Inst8< dag outs, dag ins, string asmstr, list<dag> pattern, bits<8> op = 0> :
  Inst< outs, ins, asmstr, pattern>
{
  let Size = 1;

  bits<8> opcode;
  let opcode = op;

  bits<8> Inst;
  let Inst{7-0} = opcode;
}

class Inst16< dag outs, dag ins, string asmstr, list<dag> pattern, bits<8> op = 0, bits<8> par = 0> :
  Inst< outs, ins, asmstr, pattern>
{
  let Size = 2;

  bits<8> opcode;
  bits<8> param;

  let opcode = op;
  let param = par;
  
  field bits<16> Inst;
  let Inst{7-0} = opcode;
  let Inst{15-8} = param;
}

class Inst24< dag outs, dag ins, string asmstr, list<dag> pattern, bits<8> op = 0, bits<16> par = 0> :
  Inst< outs, ins, asmstr, pattern>
{
  let Size = 3;

  bits<8> opcode;
  bits<16> param;

  let opcode = op;
  let param = par;
  
  field bits<24> Inst;
  let Inst{7-0} = opcode;
  let Inst{23-8} = param;
}

def Has6502 : Predicate<"Subtarget->has6502()">,
                         AssemblerPredicate<"Feature6502">
{
  let PredicateName = "Feature6502";
}


