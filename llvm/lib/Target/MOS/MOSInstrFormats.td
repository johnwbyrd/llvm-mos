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
class MOSInst : Instruction
{
  let Namespace = "mos";
  // DecoderNamespace - The "namespace" in which this instruction exists, on
  // targets like ARM which multiple ISA namespaces exist.
  let DecoderNamespace = "mos";
  // Mask of bits that cause an encoding to be UNPREDICTABLE.
  // If a bit is set, then if the corresponding bit in the
  // target encoding differs from its value in the "Inst" field,
  // the instruction is UNPREDICTABLE (SoftFail in abstract parlance).
  field bits<32> Unpredictable = 0;
  // SoftFail is the generic name for this field, but we alias it so
  // as to make it more obvious what it means in ARM-land.
  field bits<32> SoftFail = Unpredictable;
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
}

// A MOS opcode with default settings
def MOSOpcodeDefault : MOSOpcode< 0x00 >;

// The base class for all operands on MOS.
class MOSOperand;

// An 8-bit instruction.  Sources and targets are always implied in 
// this instruction format.
class MOSInst8< MOSOpcode opcode > : MOSInst
{
  let Size = 1;
  field bits<8> Inst;
  let Inst = opcode.Opcode;
}

class MOSOperand8< bits< 8 > operand > : MOSOperand
{
  field bits< 8 > Operand8;
  let Operand8 = operand;
}

class MOSInst16< MOSOpcode opcode, MOSOperand8 operand > : MOSInst
{
  let Size = 2;
  field bits<16> Inst;
  let Inst{0-7} = opcode.Opcode;
  let Inst{8-15} = operand.Operand8;
}

class MOSOperand16< bits< 16 > operand > : MOSOperand
{
  field bits< 16 > Operand16;
  let Operand16 = operand;
}

class MOSInst24< MOSOpcode opcode, MOSOperand16 operand > : MOSInst
{
  let Size = 3;
  field bits<24> Inst;
  let Inst{0-7} = opcode.Opcode;
  let Inst{8-23} = operand.Operand16;
}


def Has6502       :    Predicate<"Subtarget->has6502()">,
                         AssemblerPredicate<"Feature6502", "Has 6502 original core instructions">;


