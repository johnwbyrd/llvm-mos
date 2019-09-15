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

def BRK: MOSInst8< MOSOpcode< 0x00 > >
{
  let OutOperandList = (outs unknown:$dst); 
  let InOperandList = (ins unknown:$src);
  let AsmString = "brk";
  let Inst = 0x00;
}

def LDAIMM: MOSInst8< MOSOpcode< 0xa9 > >
{
  let OutOperandList = (outs); 
  let InOperandList = (ins i8imm:$src);
  let AsmString = "lda\t$src";
}