//===-- MOSInstrInfo.td - MOS Instruction defs -------------*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes the MOS instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

include "MOSInstrFormats.td"

class MOSInst : Instruction
{
  let Namespace = "mos";
  let DecoderNamespace = "6502";
  // Mask of bits that cause an encoding to be UNPREDICTABLE.
  // If a bit is set, then if the corresponding bit in the
  // target encoding differs from its value in the "Inst" field,
  // the instruction is UNPREDICTABLE (SoftFail in abstract parlance).
  field bits<32> Unpredictable = 0;
  // SoftFail is the generic name for this field, but we alias it so
  // as to make it more obvious what it means in ARM-land.
  field bits<32> SoftFail = Unpredictable;
}

class MOSInst8 : MOSInst
{
  field bits<8> Inst;
  let Inst = 0;
  let Size = 1;
}

def imm8 : SDNodeXForm<imm,
[{
  return CurDAG->getTargetConstant(N->getAPIntValue(), SDLoc(N), MVT::i8);
}]>;

def imm16 : SDNodeXForm<imm,
[{
  return CurDAG->getTargetConstant(N->getAPIntValue(), SDLoc(N), MVT::i8);
}]>;

def BRK: MOSInst8
{
  let OutOperandList = (outs unknown:$dst); 
  let InOperandList = (ins unknown:$src);
  let AsmString = "brk";
  let Inst = 0x00;
}

def LDAIMM: MOSInst8
{
  let Inst = 0xa9;
  let OutOperandList = (outs); 
  let InOperandList = (ins i8imm:$src);
  let AsmString = "lda\t$src";
}