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

def BRK: MOSInst8< 
    (outs),
    (ins),
    "brk",
    [], 
    MOSOpcode< 0x00 > >;
  // Requires<[Has6502]>;

/*
def LDAIMM: MOSInst16< 
  (outs A),
  (ins i8imm:$src),
  "lda\t$src",
  [(set A, (i8imm:$src))],
  MOSOpcode< 0xa9 > >;
  */