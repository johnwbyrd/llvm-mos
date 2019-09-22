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
class MOSInst< dag outs, dag ins, string asmstr, list<dag> pattern> : Instruction
{
  let Namespace = "MOS";

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;

  field bits<32> SoftFail = 0;
}

// All MOS-related platforms depend on an 8-bit opcode.  
// Some instructions require no operands.
// Some instructions accept an 8 bit operand, which may be an 8 bit 
// immediate value, or an 8-bit relative branch on a condition. 
// And some instructions accept a 16-bit address value.
// In the SWEET16 case only, that 16-bit value could alternatively
// be an immediate value.
class MOSOpcode< bits< 8 > opcode >
{
    field bits<8> Opcode;
    let Opcode = opcode;
}

// The base class for all operands on MOS.
class MOSAsmOperand : AsmOperandClass;

// An 8-bit instruction.  Sources and targets are always implied in 
// this instruction format.
class MOSInst8< dag outs, dag ins, string asmstr, list<dag> pattern, 
        MOSOpcode opcode > :
    MOSInst< outs, ins, asmstr, pattern>
{
  let Size = 1;
  field bits<8> Inst;
  let Inst = opcode.Opcode;
}

class MOSAsmOperand8< bits< 8 > operand > : MOSAsmOperand
{
  field bits< 8 > Operand8;
  let Operand8 = operand;
}

class MOSOpImm< bits< 8 > operand > : MOSAsmOperand8< operand >
{
    let Name = "Imm";
}

class MOSInst16< dag outs, dag ins, string asmstr, list<dag> pattern, 
    MOSOpcode opcode, MOSAsmOperand8 operand > :
  MOSInst< outs, ins, asmstr, pattern >
{
  let Size = 2;
  field bits<16> Inst;
  let Inst{0-7} = opcode.Opcode;
  let Inst{8-15} = operand.Operand8;
}

class MOSAsmOperand16< bits< 16 > operand > : MOSAsmOperand
{
  field bits< 16 > Operand16;
  let Operand16 = operand;
}

class MOSInst24< dag outs, dag ins, string asmstr, list<dag> pattern, 
    MOSOpcode opcode, MOSAsmOperand16 operand > : 
  MOSInst< outs, ins, asmstr, pattern >
{
  let Size = 3;
  field bits<24> Inst;
  let Inst{0-7} = opcode.Opcode;
  let Inst{8-23} = operand.Operand16;
}

// Predicates for subtarget features.


def Has6502 : Predicate<"Subtarget->has6502()">,
                         AssemblerPredicate<"Feature6502">
{
  let PredicateName = "Feature6502";
}


