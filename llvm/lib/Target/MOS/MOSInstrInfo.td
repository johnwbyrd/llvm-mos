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
defm ORA : CC1_All< 0b000, (outs), (ins), "ora", []>;
defm AND : CC1_All< 0b001, (outs), (ins), "and", []>;
defm EOR : CC1_All< 0b010, (outs), (ins), "eor", []>;
defm ADC : CC1_All< 0b011, (outs), (ins), "adc", []>;
defm LDA : CC1_All< 0b101, (outs), (ins), "lda", []>;
defm CMP : CC1_All< 0b110, (outs), (ins), "cmp", []>;
defm SBC : CC1_All< 0b111, (outs), (ins), "sbc", []>;

/// The STA # instruction would be nonsensical, so skip it.
defm STA : CC1_NoImmediate< 0b100, (outs), (ins), "sta", []>;

/// CC2 regular instructions
defm ASL : CC2_Shift< 0b000, (outs), (ins), "asl", []>;
defm ROL : CC2_Shift< 0b001, (outs), (ins), "rol", []>;
defm LSR : CC2_Shift< 0b010, (outs), (ins), "lsr", []>;
defm ROR : CC2_Shift< 0b011, (outs), (ins), "ror", []>;

defm STX : CC2_NonShift< 0b100, (outs), (ins), "stx", []>;
defm LDX : CC2_NonShift< 0b101, (outs), (ins), "ldx", []>;
defm DEC : CC2_NonShift< 0b110, (outs), (ins), "dec", []>;
defm INC : CC2_NonShift< 0b111, (outs), (ins), "inc", []>;

/// CC2 exceptional cases

/// Single-byte opcodes
/// subroutine instructions:

/*
BRK	JSR abs	RTI	RTS
00	20	40	60
(JSR is the only absolute-addressing instruction that doesn't fit the aaabbbcc pattern.)

Other single-byte instructions:

PHP	PLP	PHA	PLA	DEY	TAY	INY	INX
08	28	48	68	88	A8	C8	E8
CLC	SEC	CLI	SEI	TYA	CLV	CLD	SED
18	38	58	78	98	B8	D8	F8
TXA	TXS	TAX	TSX	DEX	NOP
8A	9A	AA	BA	CA	EA
*/


/*
The conditional branch instructions all have the form xxy10000. The flag indicated
by xx is compared with y, and the branch is taken if they are equal.

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

  