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
    MOSOpcode< 0x00 > >, 
    Requires<[Has6502]>;

def ORA_imm: MOSInst16<
  (outs),
  (ins i8imm:$val),
  "ora\t#val",
  [],
  MOSOpcode< 0x05 >, MOSOpImm8< 0x00 > >,
    Requires<[Has6502]>;


def LDA_imm: MOSInst16< 
  (outs),
  (ins i8imm:$val),
  "lda\t$val",
  [],
  MOSOpcode< 0xa9 >, MOSOpImm8< 0x00 > >,
  Requires<[Has6502]>;
  