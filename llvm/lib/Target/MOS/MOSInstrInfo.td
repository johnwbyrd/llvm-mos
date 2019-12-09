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

/* 
For cc = 01, that is, OpcodeC1 type instructions
aaa	opcode
000	ORA
001	AND
010	EOR
011	ADC
100	STA
101	LDA
110	CMP
111	SBC
*/

defm ORA : CC1_All< 0b000, (outs), (ins), "ora", []>;
defm AND : CC1_All< 0b001, (outs), (ins), "and", []>;
defm EOR : CC1_All< 0b010, (outs), (ins), "eor", []>;
defm ADC : CC1_All< 0b011, (outs), (ins), "adc", []>;
defm LDA : CC1_All< 0b101, (outs), (ins), "lda", []>;
defm CMP : CC1_All< 0b110, (outs), (ins), "cmp", []>;
defm SBC : CC1_All< 0b111, (outs), (ins), "sbc", []>;

/* The STA # instruction would be nonsensical, so skip it. */
defm STA : CC1_NoImmediate< 0b100, (outs), (ins), "sta", []>;

/*

The conditional branch instructions all have the form xxy10000. The flag indicated by xx is compared with y, and the branch is taken if they are equal.

xx	flag
00	negative
01	overflow
10	carry
11	zero
This gives the following branches:

BPL	BMI	BVC	BVS	BCC	BCS	BNE	BEQ
10	30	50	70	90	B0	D0	F0
*/
def BPL : ConditionalBranch< "bpl", (outs), (ins), [], 0b00, 0b0 >;
def BMI : ConditionalBranch< "bmi", (outs), (ins), [], 0b00, 0b1 >;
def BVC : ConditionalBranch< "bvc", (outs), (ins), [], 0b01, 0b0 >;
def BVS : ConditionalBranch< "bvs", (outs), (ins), [], 0b01, 0b1 >;
def BCC : ConditionalBranch< "bcc", (outs), (ins), [], 0b10, 0b0 >;
def BCS : ConditionalBranch< "bcs", (outs), (ins), [], 0b10, 0b1 >;
def BNE : ConditionalBranch< "bne", (outs), (ins), [], 0b11, 0b0 >;
def BEQ : ConditionalBranch< "beq", (outs), (ins), [], 0b11, 0b1 >;



def BRK: Inst8< 
    (outs),
    (ins),
    "brk",
    [] >;
}

  