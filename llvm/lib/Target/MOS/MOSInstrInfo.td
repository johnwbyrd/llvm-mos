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

let Predicates = [Has6502] in 
{
def BRK: Inst8< 
    (outs),
    (ins),
    "brk",
    [] >;

def ASL_imm: Inst8< 
    (outs),
    (ins),
    "asl",
    [], 
    0x0a>;

def LDA_imm: Inst16< 
  (outs),
  (ins imm8:$val),
  "lda\t$val",
  [],
  0xa9>
  {
      bits<8> val;
      let param = val;
  }

}

  