//===-- MOSInstr65CE02.td - MOS 65CE02 Family Instructions --*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes 65CE02-based instructions in TableGen format.
// Includes 65CE02, 4510, and 45GS02 (MEGA65) variants.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// 65CE02 Instructions
//===----------------------------------------------------------------------===//

let Predicates = [Has65CE02] in {
let DecoderNamespace = "65ce02" in {

// CLE - Clear extended stack disable: affects E flag (extension)
let Cycles6502 = 2 in {
defm CLE           : Op<0x02, "cle", Implicit>;
}

// SEE - Set extended stack disable: affects E flag (extension)
let Cycles6502 = 2 in {
defm SEE           : Op<0x03, "see", Implicit>;
}

// NEG - Negate accumulator: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm NEG           : Op<0x42, "neg", Implicit>;
}

// ASR - Arithmetic shift right: affects N, Z, C
let Cycles6502 = 2, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASR           : Op<0x43, "asr", Implicit>;
}

// Transfer instructions - affect N, Z from transferred value
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TAZ           : Op<0x4B, "taz", Implicit>;
defm TZA           : Op<0x6B, "tza", Implicit>;
defm TBA           : Op<0x7B, "tba", Implicit>;
defm TSY           : Op<0x0B, "tsy", Implicit>;
}

// TAB - Transfer A to B: no flags (B is base page register)
let Cycles6502 = 2 in {
defm TAB           : Op<0x5B, "tab", Implicit>;
}

// TYS - Transfer Y to S: no flags (stack pointer)
let Cycles6502 = 2 in {
defm TYS           : Op<0x2B, "tys", Implicit>;
}

// INZ/DEZ - Increment/Decrement Z register: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm INZ           : Op<0x1B, "inz", Implicit>;
defm DEZ           : Op<0x3B, "dez", Implicit>;
}

// PHZ - Push Z register: no flags
let Cycles6502 = 3 in {
defm PHZ           : Op<0xDB, "phz", Implicit>;
}

// PLZ - Pull Z register: affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm PLZ           : Op<0xFB, "plz", Implicit>;
}

// LDZ - Load Z register: affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm LDZ           : Op<0xA3, "ldz", Immediate>;
defm LDZ           : Op<0xAB, "ldz", Absolute>;
}

let Cycles6502 = 4, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm LDZ           : Op<0xBB, "ldz", AbsoluteX>;
}

// CPZ - Compare Z register: affects N, Z, C
let Cycles6502 = 3, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CPZ           : Op<0xC2, "cpz", Immediate>;
}

let Cycles6502 = 4, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CPZ           : Op<0xD4, "cpz", ZeroPage>;
defm CPZ           : Op<0xDC, "cpz", Absolute>;
}

// ASR - Zero Page: affects N, Z, C
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASR           : Op<0x44, "asr", ZeroPage>;
}

let Cycles6502 = 6, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASR           : Op<0x54, "asr", ZeroPageX>;
}

// DEW/INW - Word operations: affects N, Z (16-bit result)
let Cycles6502 = 6, FlagN = 1, FlagZ = 1 in {
defm DEW           : Op<0xC3, "dew", ZeroPage>;
defm INW           : Op<0xE3, "inw", ZeroPage>;
}

// ASW - Arithmetic shift word left: affects N, Z, C
let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASW           : Op<0xCB, "asw", Absolute>;
}

// ROW - Rotate word left: affects N, Z, C
let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ROW           : Op<0xEB, "row", Absolute>;
}

// Logic operations (Indirect Zero Page Z) - affect N, Z
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x12, "ora", IndirectZeroPageZ>;
defm AND           : Op<0x32, "and", IndirectZeroPageZ>;
defm EOR           : Op<0x52, "eor", IndirectZeroPageZ>;
defm LDA           : Op<0xB2, "lda", IndirectZeroPageZ>;
}

// Arithmetic (Indirect Zero Page Z) - affect N, V, Z, C
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x72, "adc", IndirectZeroPageZ>;
defm SBC           : Op<0xF2, "sbc", IndirectZeroPageZ>;
}

// Compare (Indirect Zero Page Z) - affect N, Z, C
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xD2, "cmp", IndirectZeroPageZ>;
}

// STA - stores, no flags
let Cycles6502 = 5 in {
defm STA           : Op<0x92, "sta", IndirectZeroPageZ>;
}

// Extended stores - no flags
let Cycles6502 = 5 in {
defm STY           : Op<0x8B, "sty", AbsoluteX>;
defm STX           : Op<0x9B, "stx", AbsoluteY>;
}

// Stack relative - stores no flags
let Cycles6502 = 7 in {
def STA_IndirectStackRelativeY_CE02 : MOSInst<0x82, "sta", IndirectStackRelativeY>;
}

// Stack relative - load affects N, Z
let Cycles6502 = 7, FlagN = 1, FlagZ = 1 in {
def LDA_IndirectStackRelativeY_CE02 : MOSInst<0xE2, "lda", IndirectStackRelativeY>;
}

// PHW - Push word: no flags
let Cycles6502 = 4 in {
def PHW_Immediate  : MOSInst<0xF4, "phw", Immediate16>;
}

let Cycles6502 = 5 in {
defm PHW           : Op<0xFC, "phw", Absolute>;
}

// 16-bit relative branches - no flags
let Cycles6502 = 3, PageCrossCycles = 1 in {
defm BPL           : OpBranch16<0x13, "bpl">;
defm BMI           : OpBranch16<0x33, "bmi">;
defm BVC           : OpBranch16<0x53, "bvc">;
defm BVS           : OpBranch16<0x73, "bvs">;
defm BCC           : OpBranch16<0x93, "bcc">;
defm BCS           : OpBranch16<0xB3, "bcs">;
defm BNE           : OpBranch16<0xD3, "bne">;
defm BEQ           : OpBranch16<0xF3, "beq">;
}

// BRA - unconditional branch: no flags
let Cycles6502 = 4, PageCrossCycles = 1 in {
defm BRA           : OpJump<0x83, "bra", Relative16>;
}

// BSR - branch subroutine: no flags
let Cycles6502 = 8 in {
defm BSR           : OpCall<0x63, "bsr", Relative16>;
}

// JSR indirect variants: no flags
let Cycles6502 = 6 in {
def JSR_AbsoluteIndirect  : MOSInst<0x22, "jsr", Indirect16>, TraitCall { let isIndirectBranch = true; }
}

let Cycles6502 = 8 in {
def JSR_AbsoluteIndirectX : MOSInst<0x23, "jsr", IndexedIndirect16>, TraitCall { let isIndirectBranch = true; }
}

// RTN - return with cleanup: no flags
let Cycles6502 = 7 in {
defm RTN           : OpReturnWithMode<0x62, "rtn", Immediate>;
}

} // DecoderNamespace = "65ce02"
} // Predicates = [Has65CE02]

//===----------------------------------------------------------------------===//
// 4510 Instructions
//===----------------------------------------------------------------------===//

let Predicates = [Has4510] in {
let DecoderNamespace = "65ce02" in {

// MAP - Memory mapping: no flags
let Cycles6502 = 2 in {
defm MAP           : Op<0x5C, "map", Implicit>;
}

} // DecoderNamespace = "65ce02"

def : MnemonicAlias<"eom", "nop">;

} // Predicates = [Has4510]

//===----------------------------------------------------------------------===//
// 45GS02 Instructions (MEGA65)
//===----------------------------------------------------------------------===//
// Note: The Q pseudo-register instructions operate on 32-bit values.
// Flag effects mirror their 8-bit counterparts.

let Predicates = [Has45GS02] in {
let DecoderNamespace = "45gs02" in {

// Q register shifts/rotates (implicit) - affect N, Z, C
let Cycles6502 = 3, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def ASLQ_Implicit  : Inst4502NegNegImplicit<"aslq", Opcode<0x0A>, Implicit>;
def ROLQ_Implicit  : Inst4502NegNegImplicit<"rolq", Opcode<0x2A>, Implicit>;
def ASRQ_Implicit  : Inst4502NegNegImplicit<"asrq", Opcode<0x43>, Implicit>;
def LSRQ_Implicit  : Inst4502NegNegImplicit<"lsrq", Opcode<0x4A>, Implicit>;
def RORQ_Implicit  : Inst4502NegNegImplicit<"rorq", Opcode<0x6A>, Implicit>;
}

// Q register inc/dec (implicit) - affect N, Z
let Cycles6502 = 3, FlagN = 1, FlagZ = 1 in {
def INQ_Implicit   : Inst4502NegNegImplicit<"inq", Opcode<0x1A>, Implicit>;
def DEQ_Implicit   : Inst4502NegNegImplicit<"deq", Opcode<0x3A>, Implicit>;
}

// Base page quad logic - affect N, Z
let Cycles6502 = 6, FlagN = 1, FlagZ = 1 in {
def ORQ_BasePage   : Inst4502NegNeg8<"orq", Opcode<0x05>, ZeroPage>;
def ANDQ_BasePage  : Inst4502NegNeg8<"andq", Opcode<0x25>, ZeroPage>;
def EORQ_BasePage  : Inst4502NegNeg8<"eorq", Opcode<0x45>, ZeroPage>;
def LDQ_BasePage   : Inst4502NegNeg8<"ldq", Opcode<0xA5>, ZeroPage>;
}

// BITQ - affects N, V, Z
let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagZ = 1 in {
def BITQ_BasePage  : Inst4502NegNeg8<"bitq", Opcode<0x24>, ZeroPage>;
}

// Base page quad arithmetic - affect N, V, Z, C
let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def ADCQ_BasePage  : Inst4502NegNeg8<"adcq", Opcode<0x65>, ZeroPage>;
def SBCQ_BasePage  : Inst4502NegNeg8<"sbcq", Opcode<0xE5>, ZeroPage>;
}

// Base page quad compare - affect N, Z, C
let Cycles6502 = 6, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def CMPQ_BasePage  : Inst4502NegNeg8<"cmpq", Opcode<0xC5>, ZeroPage>;
}

// STQ - store, no flags
let Cycles6502 = 6 in {
def STQ_BasePage   : Inst4502NegNeg8<"stq", Opcode<0x85>, ZeroPage>;
}

// Base page quad RMW shifts - affect N, Z, C
let Cycles6502 = 8, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def ASLQ_BasePage  : Inst4502NegNeg8<"aslq", Opcode<0x06>, ZeroPage>;
def ROLQ_BasePage  : Inst4502NegNeg8<"rolq", Opcode<0x26>, ZeroPage>;
def ASRQ_BasePage  : Inst4502NegNeg8<"asrq", Opcode<0x44>, ZeroPage>;
def LSRQ_BasePage  : Inst4502NegNeg8<"lsrq", Opcode<0x46>, ZeroPage>;
def RORQ_BasePage  : Inst4502NegNeg8<"rorq", Opcode<0x66>, ZeroPage>;
}

// Base page quad RMW inc/dec - affect N, Z
let Cycles6502 = 8, FlagN = 1, FlagZ = 1 in {
def DEQ_BasePage   : Inst4502NegNeg8<"deq", Opcode<0xC6>, ZeroPage>;
def INQ_BasePage   : Inst4502NegNeg8<"inq", Opcode<0xE6>, ZeroPage>;
}

// Indirect quad logic - affect N, Z
let Cycles6502 = 8, FlagN = 1, FlagZ = 1 in {
def ORQ_Indirect   : Inst4502NegNeg8<"orq", Opcode<0x12>, Indirect>;
def ANDQ_Indirect  : Inst4502NegNeg8<"andq", Opcode<0x32>, Indirect>;
def EORQ_Indirect  : Inst4502NegNeg8<"eorq", Opcode<0x52>, Indirect>;
def LDQ_IndirectZeroPageZ : Inst4502NegNeg8<"ldq", Opcode<0xB2>, IndirectZeroPageZ>;
}

// Indirect quad arithmetic - affect N, V, Z, C
let Cycles6502 = 8, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def ADCQ_Indirect  : Inst4502NegNeg8<"adcq", Opcode<0x72>, Indirect>;
def SBCQ_Indirect  : Inst4502NegNeg8<"sbcq", Opcode<0xF2>, Indirect>;
}

// Indirect quad compare - affect N, Z, C
let Cycles6502 = 8, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def CMPQ_Indirect  : Inst4502NegNeg8<"cmpq", Opcode<0xD2>, Indirect>;
}

// STQ indirect - no flags
let Cycles6502 = 8 in {
def STQ_Indirect   : Inst4502NegNeg8<"stq", Opcode<0x92>, Indirect>;
}

// Base page indexed quad shifts - affect N, Z, C
let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def ASLQ_BasePageX : Inst4502NegNeg8<"aslq", Opcode<0x16>, ZeroPageX>;
def ROLQ_BasePageX : Inst4502NegNeg8<"rolq", Opcode<0x36>, ZeroPageX>;
def ASRQ_BasePageX : Inst4502NegNeg8<"asrq", Opcode<0x54>, ZeroPageX>;
def LSRQ_BasePageX : Inst4502NegNeg8<"lsrq", Opcode<0x56>, ZeroPageX>;
def RORQ_BasePageX : Inst4502NegNeg8<"rorq", Opcode<0x76>, ZeroPageX>;
}

// Base page indexed quad inc/dec - affect N, Z
let Cycles6502 = 7, FlagN = 1, FlagZ = 1 in {
def DEQ_BasePageX  : Inst4502NegNeg8<"deq", Opcode<0xD6>, ZeroPageX>;
def INQ_BasePageX  : Inst4502NegNeg8<"inq", Opcode<0xF6>, ZeroPageX>;
}

// Absolute quad logic - affect N, Z
let Cycles6502 = 7, FlagN = 1, FlagZ = 1 in {
def ORQ_Absolute   : Inst4502NegNeg16<"orq", Opcode<0x0D>, Absolute>;
def ANDQ_Absolute  : Inst4502NegNeg16<"andq", Opcode<0x2D>, Absolute>;
def EORQ_Absolute  : Inst4502NegNeg16<"eorq", Opcode<0x4D>, Absolute>;
def LDQ_Absolute   : Inst4502NegNeg16<"ldq", Opcode<0xAD>, Absolute>;
}

// BITQ absolute - affects N, V, Z
let Cycles6502 = 7, FlagN = 1, FlagV = 1, FlagZ = 1 in {
def BITQ_Absolute  : Inst4502NegNeg16<"bitq", Opcode<0x2C>, Absolute>;
}

// Absolute quad arithmetic - affect N, V, Z, C
let Cycles6502 = 7, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def ADCQ_Absolute  : Inst4502NegNeg16<"adcq", Opcode<0x6D>, Absolute>;
def SBCQ_Absolute  : Inst4502NegNeg16<"sbcq", Opcode<0xED>, Absolute>;
}

// Absolute quad compare - affect N, Z, C
let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def CMPQ_Absolute  : Inst4502NegNeg16<"cmpq", Opcode<0xCD>, Absolute>;
}

// STQ absolute - no flags
let Cycles6502 = 7 in {
def STQ_Absolute   : Inst4502NegNeg16<"stq", Opcode<0x8D>, Absolute>;
}

// Absolute quad RMW shifts - affect N, Z, C
let Cycles6502 = 9, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def ASLQ_Absolute  : Inst4502NegNeg16<"aslq", Opcode<0x0E>, Absolute>;
def ROLQ_Absolute  : Inst4502NegNeg16<"rolq", Opcode<0x2E>, Absolute>;
def LSRQ_Absolute  : Inst4502NegNeg16<"lsrq", Opcode<0x4E>, Absolute>;
def RORQ_Absolute  : Inst4502NegNeg16<"rorq", Opcode<0x6E>, Absolute>;
}

// Absolute quad RMW inc/dec - affect N, Z
let Cycles6502 = 9, FlagN = 1, FlagZ = 1 in {
def DEQ_Absolute   : Inst4502NegNeg16<"deq", Opcode<0xCE>, Absolute>;
def INQ_Absolute   : Inst4502NegNeg16<"inq", Opcode<0xEE>, Absolute>;
}

// Absolute,X quad RMW shifts - affect N, Z, C
let Cycles6502 = 10, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def ASLQ_AbsoluteX : Inst4502NegNeg16<"aslq", Opcode<0x1E>, AbsoluteX>;
def ROLQ_AbsoluteX : Inst4502NegNeg16<"rolq", Opcode<0x3E>, AbsoluteX>;
def LSRQ_AbsoluteX : Inst4502NegNeg16<"lsrq", Opcode<0x5E>, AbsoluteX>;
def RORQ_AbsoluteX : Inst4502NegNeg16<"rorq", Opcode<0x7E>, AbsoluteX>;
}

// Absolute,X quad RMW inc/dec - affect N, Z
let Cycles6502 = 10, FlagN = 1, FlagZ = 1 in {
def DEQ_AbsoluteX  : Inst4502NegNeg16<"deq", Opcode<0xDE>, AbsoluteX>;
def INQ_AbsoluteX  : Inst4502NegNeg16<"inq", Opcode<0xFE>, AbsoluteX>;
}

// Flat memory logic - affect N, Z
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
def ORA_FlatIndirectZeroPageZ : Inst4502Nop16<"ora", Opcode<0x12>, IndirectIndexedLongZ>;
def AND_FlatIndirectZeroPageZ : Inst4502Nop16<"and", Opcode<0x32>, IndirectIndexedLongZ>;
def EOR_FlatIndirectZeroPageZ : Inst4502Nop16<"eor", Opcode<0x52>, IndirectIndexedLongZ>;
def LDA_FlatIndirectZeroPageZ : Inst4502Nop16<"lda", Opcode<0xB2>, IndirectIndexedLongZ>;
}

// Flat memory arithmetic - affect N, V, Z, C
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def ADC_FlatIndirectZeroPageZ : Inst4502Nop16<"adc", Opcode<0x72>, IndirectIndexedLongZ>;
def SBC_FlatIndirectZeroPageZ : Inst4502Nop16<"sbc", Opcode<0xF2>, IndirectIndexedLongZ>;
}

// Flat memory compare - affect N, Z, C
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def CMP_FlatIndirectZeroPageZ : Inst4502Nop16<"cmp", Opcode<0xD2>, IndirectIndexedLongZ>;
}

// Flat memory store - no flags
let Cycles6502 = 6 in {
def STA_FlatIndirectZeroPageZ : Inst4502Nop16<"sta", Opcode<0x92>, IndirectIndexedLongZ>;
}

// Quad indirect long logic - affect N, Z
let Cycles6502 = 9, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
def ORQ_IndirectLong  : Inst4502NegNegNop8<"orq", Opcode<0x12>, IndirectLong>;
def ANDQ_IndirectLong : Inst4502NegNegNop8<"andq", Opcode<0x32>, IndirectLong>;
def EORQ_IndirectLong : Inst4502NegNegNop8<"eorq", Opcode<0x52>, IndirectLong>;
def LDQ_IndirectIndexedLongZ : Inst4502NegNegNop8<"ldq", Opcode<0xB2>, IndirectIndexedLongZ>;
}

// Quad indirect long arithmetic - affect N, V, Z, C
let Cycles6502 = 9, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def ADCQ_IndirectLong : Inst4502NegNegNop8<"adcq", Opcode<0x72>, IndirectLong>;
def SBCQ_IndirectLong : Inst4502NegNegNop8<"sbcq", Opcode<0xF2>, IndirectLong>;
}

// Quad indirect long compare - affect N, Z, C
let Cycles6502 = 9, PageCrossCycles = 1, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def CMPQ_IndirectLong : Inst4502NegNegNop8<"cmpq", Opcode<0xD2>, IndirectLong>;
}

// Quad indirect long store - no flags
let Cycles6502 = 9 in {
def STQ_IndirectLong  : Inst4502NegNegNop8<"stq", Opcode<0x92>, IndirectLong>;
}

} // DecoderNamespace = "45gs02"

def : MnemonicAlias<"cpq", "cmpq">;

} // Predicates = [Has45GS02]
