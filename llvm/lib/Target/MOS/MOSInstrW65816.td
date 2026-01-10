//===-- MOSInstrW65816.td - MOS W65816 Family Instructions --*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes W65816 and 65EL02 instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// W65816 and 65EL02 Common Instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasW65816Or65EL02] in {

// 16-bit accumulator immediate instructions (M flag = 0)
// Logic: affect N, Z
let Cycles6502 = 3, FlagN = 1, FlagZ = 1 in {
def ORA_Immediate16 : MOSInst<0x09, "ora", Immediate16> {
  let DecoderNamespace = "MOSMLow";
  let MLow = 1;
}
def AND_Immediate16 : MOSInst<0x29, "and", Immediate16> {
  let DecoderNamespace = "MOSMLow";
  let MLow = 1;
}
def EOR_Immediate16 : MOSInst<0x49, "eor", Immediate16> {
  let DecoderNamespace = "MOSMLow";
  let MLow = 1;
}
def LDA_Immediate16 : MOSInst<0xA9, "lda", Immediate16> {
  let DecoderNamespace = "MOSMLow";
  let MLow = 1;
}
}

// Arithmetic: affect N, V, Z, C
let Cycles6502 = 3, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def ADC_Immediate16 : MOSInst<0x69, "adc", Immediate16> {
  let DecoderNamespace = "MOSMLow";
  let MLow = 1;
}
def SBC_Immediate16 : MOSInst<0xE9, "sbc", Immediate16> {
  let DecoderNamespace = "MOSMLow";
  let MLow = 1;
}
}

// BIT 16-bit immediate: affects Z only (not N, V like memory BIT)
let Cycles6502 = 3, FlagZ = 1 in {
def BIT_Immediate16 : MOSInst<0x89, "bit", Immediate16> {
  let DecoderNamespace = "MOSMLow";
  let MLow = 1;
}
}

// Compare: affects N, Z, C
let Cycles6502 = 3, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def CMP_Immediate16 : MOSInst<0xC9, "cmp", Immediate16> {
  let DecoderNamespace = "MOSMLow";
  let MLow = 1;
}
}

// 16-bit index register immediate instructions (X flag = 0)
// Load: affects N, Z
let Cycles6502 = 3, FlagN = 1, FlagZ = 1 in {
def LDY_Immediate16 : MOSInst<0xA0, "ldy", Immediate16> {
  let DecoderNamespace = "MOSXLow";
  let XLow = 1;
}
def LDX_Immediate16 : MOSInst<0xA2, "ldx", Immediate16> {
  let DecoderNamespace = "MOSXLow";
  let XLow = 1;
}
}

// Compare: affects N, Z, C
let Cycles6502 = 3, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def CPY_Immediate16 : MOSInst<0xC0, "cpy", Immediate16> {
  let DecoderNamespace = "MOSXLow";
  let XLow = 1;
}
def CPX_Immediate16 : MOSInst<0xE0, "cpx", Immediate16> {
  let DecoderNamespace = "MOSXLow";
  let XLow = 1;
}
}

// Stack-relative logic: affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x03, "ora", StackRelative>;
defm AND           : Op<0x23, "and", StackRelative>;
defm EOR           : Op<0x43, "eor", StackRelative>;
defm LDA           : Op<0xA3, "lda", StackRelative>;
}

// Stack-relative arithmetic: affects N, V, Z, C
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x63, "adc", StackRelative>;
defm SBC           : Op<0xE3, "sbc", StackRelative>;
}

// Stack-relative compare: affects N, Z, C
let Cycles6502 = 4, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xC3, "cmp", StackRelative>;
}

// Stack-relative stores: no flags
let Cycles6502 = 4 in {
defm STA           : Op<0x83, "sta", StackRelative>;
}

// Stack-relative indirect indexed logic: affects N, Z
let Cycles6502 = 7, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x13, "ora", IndirectStackRelativeY>;
defm AND           : Op<0x33, "and", IndirectStackRelativeY>;
defm EOR           : Op<0x53, "eor", IndirectStackRelativeY>;
defm LDA           : Op<0xB3, "lda", IndirectStackRelativeY>;
}

// Stack-relative indirect indexed arithmetic: affects N, V, Z, C
let Cycles6502 = 7, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x73, "adc", IndirectStackRelativeY>;
defm SBC           : Op<0xF3, "sbc", IndirectStackRelativeY>;
}

// Stack-relative indirect indexed compare: affects N, Z, C
let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xD3, "cmp", IndirectStackRelativeY>;
}

// Stack-relative indirect indexed stores: no flags
let Cycles6502 = 7 in {
defm STA           : Op<0x93, "sta", IndirectStackRelativeY>;
}

// Transfer instructions: TXY/TYX affect N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TXY           : Op<0x9B, "txy", Implicit>;
defm TYX           : Op<0xBB, "tyx", Implicit>;
}

// XBA - Exchange B and A: affects N, Z (based on new A value)
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm XBA           : Op<0xEB, "xba", Implicit>;
}

// XCE - Exchange Carry and Emulation: affects C, E (special)
let Cycles6502 = 2, FlagC = 1 in {
defm XCE           : Op<0xFB, "xce", Implicit>;
}

// REP/SEP - Modify processor status: affects selected flags (special)
// These can modify any flag based on the immediate operand
let Cycles6502 = 3 in {
defm REP           : Op<0xC2, "rep", Immediate>;
defm SEP           : Op<0xE2, "sep", Immediate>;
}

// PEI/PEA - Push effective: no flags
let Cycles6502 = 6 in {
defm PEI           : Op<0xD4, "pei", Indirect>;
}

let Cycles6502 = 5 in {
defm PEA           : Op<0xF4, "pea", Absolute>;
}

// JSR indexed indirect: no flags
let Cycles6502 = 8 in {
defm JSR           : OpCallIndirect<0xFC, "jsr", IndexedIndirect16>;
}

} // Predicates = [HasW65816Or65EL02]

//===----------------------------------------------------------------------===//
// W65816 Instructions
//===----------------------------------------------------------------------===//

let Predicates = [HasW65816] in {
let DecoderNamespace = "w65816" in {

// Indirect Long logic [dp]: affects N, Z
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x07, "ora", IndirectLong>;
defm AND           : Op<0x27, "and", IndirectLong>;
defm EOR           : Op<0x47, "eor", IndirectLong>;
defm LDA           : Op<0xA7, "lda", IndirectLong>;
}

// Indirect Long arithmetic [dp]: affects N, V, Z, C
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x67, "adc", IndirectLong>;
defm SBC           : Op<0xE7, "sbc", IndirectLong>;
}

// Indirect Long compare [dp]: affects N, Z, C
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xC7, "cmp", IndirectLong>;
}

// Indirect Long stores [dp]: no flags
let Cycles6502 = 6 in {
defm STA           : Op<0x87, "sta", IndirectLong>;
}

// Indirect Indexed Long logic [dp],y: affects N, Z
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x17, "ora", IndirectIndexedLong>;
defm AND           : Op<0x37, "and", IndirectIndexedLong>;
defm EOR           : Op<0x57, "eor", IndirectIndexedLong>;
defm LDA           : Op<0xB7, "lda", IndirectIndexedLong>;
}

// Indirect Indexed Long arithmetic [dp],y: affects N, V, Z, C
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x77, "adc", IndirectIndexedLong>;
defm SBC           : Op<0xF7, "sbc", IndirectIndexedLong>;
}

// Indirect Indexed Long compare [dp],y: affects N, Z, C
let Cycles6502 = 6, PageCrossCycles = 1, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xD7, "cmp", IndirectIndexedLong>;
}

// Indirect Indexed Long stores [dp],y: no flags
let Cycles6502 = 6 in {
defm STA           : Op<0x97, "sta", IndirectIndexedLong>;
}

// Absolute Long logic: affects N, Z
let Cycles6502 = 5, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x0F, "ora", AbsoluteLong>;
defm AND           : Op<0x2F, "and", AbsoluteLong>;
defm EOR           : Op<0x4F, "eor", AbsoluteLong>;
defm LDA           : Op<0xAF, "lda", AbsoluteLong>;
}

// Absolute Long arithmetic: affects N, V, Z, C
let Cycles6502 = 5, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x6F, "adc", AbsoluteLong>;
defm SBC           : Op<0xEF, "sbc", AbsoluteLong>;
}

// Absolute Long compare: affects N, Z, C
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xCF, "cmp", AbsoluteLong>;
}

// Absolute Long stores: no flags
let Cycles6502 = 5 in {
defm STA           : Op<0x8F, "sta", AbsoluteLong>;
}

// Absolute Long,X logic: affects N, Z
let Cycles6502 = 5, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x1F, "ora", AbsoluteXLong>;
defm AND           : Op<0x3F, "and", AbsoluteXLong>;
defm EOR           : Op<0x5F, "eor", AbsoluteXLong>;
defm LDA           : Op<0xBF, "lda", AbsoluteXLong>;
}

// Absolute Long,X arithmetic: affects N, V, Z, C
let Cycles6502 = 5, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x7F, "adc", AbsoluteXLong>;
defm SBC           : Op<0xFF, "sbc", AbsoluteXLong>;
}

// Absolute Long,X compare: affects N, Z, C
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xDF, "cmp", AbsoluteXLong>;
}

// Absolute Long,X stores: no flags
let Cycles6502 = 5 in {
defm STA           : Op<0x9F, "sta", AbsoluteXLong>;
}

// Stack push operations: no flags
let Cycles6502 = 4 in {
defm PHD           : Op<0x0B, "phd", Implicit>;
defm PHB           : Op<0x8B, "phb", Implicit>;
defm PHK           : Op<0x4B, "phk", Implicit>;
}

// PLD - Pull Direct Page: affects N, Z
let Cycles6502 = 5, FlagN = 1, FlagZ = 1 in {
defm PLD           : Op<0x2B, "pld", Implicit>;
}

// PLB - Pull Data Bank: affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm PLB           : Op<0xAB, "plb", Implicit>;
}

// TCS - Transfer C to S: no flags
let Cycles6502 = 2 in {
defm TCS           : Op<0x1B, "tcs", Implicit>;
}

// TSC - Transfer S to C: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TSC           : Op<0x3B, "tsc", Implicit>;
}

// TCD - Transfer C to D: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TCD           : Op<0x5B, "tcd", Implicit>;
}

// TDC - Transfer D to C: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TDC           : Op<0x7B, "tdc", Implicit>;
}

// WDM - Reserved (2-byte NOP): no flags
let Cycles6502 = 2 in {
defm WDM           : Op<0x42, "wdm", Immediate>;
}

// Block move: no flags
let Cycles6502 = 7 in {
def MVP_816MemoryMove : Inst816MemoryMove<"mvp", Opcode<0x44>>;
def MVN_816MemoryMove : Inst816MemoryMove<"mvn", Opcode<0x54>>;
}

// JMP absolute long: no flags
let Cycles6502 = 4 in {
defm JMP           : OpJump<0x5C, "jmp", AbsoluteLong>;
}

// PER - Push effective relative: no flags
let Cycles6502 = 6 in {
def PER_Relative16 : MOSInst<0x62, "per", Relative16>;
}

// RTL - Return long: no flags
let Cycles6502 = 6 in {
defm RTL           : OpReturn<0x6B, "rtl">;
}

// BRL - Branch relative long: no flags
let Cycles6502 = 4 in {
defm BRL           : OpJump<0x82, "brl", Relative16>;
}

// JSL - Jump subroutine long: no flags
let Cycles6502 = 8 in {
defm JSL           : OpCall<0x22, "jsl", AbsoluteLong>;
}

// JML - Jump indirect long: no flags
let Cycles6502 = 6 in {
defm JML           : OpJumpIndirect<0xDC, "jml", Indirect16>;
}

} // DecoderNamespace = "w65816"
} // Predicates = [HasW65816]

//===----------------------------------------------------------------------===//
// 65EL02 Instructions
//===----------------------------------------------------------------------===//

let Predicates = [Has65EL02] in {
let DecoderNamespace = "65el02" in {

// Various 65EL02-specific implied operations
// Most transfers affect N, Z; others are special-purpose

// NXT/ENT - no flags (control flow)
let Cycles6502 = 2 in {
defm NXT           : Op<0x02, "nxt", Implicit>;
defm ENT           : Op<0x22, "ent", Implicit>;
}

// NXA - next accumulator: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm NXA           : Op<0x42, "nxa", Implicit>;
}

// TXI/TIX - Transfer X/I: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TXI           : Op<0x5C, "txi", Implicit>;
defm TIX           : Op<0xDC, "tix", Implicit>;
}

// RLA - Rotate left accumulator: affects N, Z, C
let Cycles6502 = 2, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm RLA           : Op<0x6B, "rla", Implicit>;
}

// TXR/TRX - Transfer X/R: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TXR           : Op<0x8B, "txr", Implicit>;
defm TRX           : Op<0xAB, "trx", Implicit>;
}

// ZEA - Zero extend A: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm ZEA           : Op<0x8F, "zea", Implicit>;
}

// SEA - Sign extend A: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm SEA           : Op<0x9F, "sea", Implicit>;
}

// TDA/TAD - Transfer D/A: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TDA           : Op<0xAF, "tda", Implicit>;
defm TAD           : Op<0xBF, "tad", Implicit>;
}

// R-stack push operations: no flags
let Cycles6502 = 3 in {
defm RHI           : Op<0x0B, "rhi", Implicit>;
defm RHX           : Op<0x1B, "rhx", Implicit>;
defm RHA           : Op<0x4B, "rha", Implicit>;
defm RHY           : Op<0x5B, "rhy", Implicit>;
}

// R-stack pull operations: affect N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm RLI           : Op<0x2B, "rli", Implicit>;
defm RLX           : Op<0x3B, "rlx", Implicit>;
defm RLY           : Op<0x7B, "rly", Implicit>;
}

// R-stack relative logic: affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x07, "ora", RStackRelative>;
defm AND           : Op<0x27, "and", RStackRelative>;
defm EOR           : Op<0x47, "eor", RStackRelative>;
defm LDA           : Op<0xA7, "lda", RStackRelative>;
}

// R-stack relative arithmetic: affects N, V, Z, C
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x67, "adc", RStackRelative>;
defm SBC           : Op<0xE7, "sbc", RStackRelative>;
}

// R-stack relative compare: affects N, Z, C
let Cycles6502 = 4, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xC7, "cmp", RStackRelative>;
}

// R-stack relative stores: no flags
let Cycles6502 = 4 in {
defm STA           : Op<0x87, "sta", RStackRelative>;
}

// R-stack relative indirect indexed logic: affects N, Z
let Cycles6502 = 7, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x17, "ora", IndirectRStackRelativeY>;
defm AND           : Op<0x37, "and", IndirectRStackRelativeY>;
defm EOR           : Op<0x57, "eor", IndirectRStackRelativeY>;
defm LDA           : Op<0xB7, "lda", IndirectRStackRelativeY>;
}

// R-stack relative indirect indexed arithmetic: affects N, V, Z, C
let Cycles6502 = 7, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x77, "adc", IndirectRStackRelativeY>;
defm SBC           : Op<0xF7, "sbc", IndirectRStackRelativeY>;
}

// R-stack relative indirect indexed compare: affects N, Z, C
let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xD7, "cmp", IndirectRStackRelativeY>;
}

// R-stack relative indirect indexed stores: no flags
let Cycles6502 = 7 in {
defm STA           : Op<0x97, "sta", IndirectRStackRelativeY>;
}

// MUL - affects N, Z, C (result in A:Y)
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def MUL_ZeroPage_EL02  : MOSInst<0x0F, "mul", ZeroPage>;
}

let Cycles6502 = 6, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def MUL_ZeroPageX_EL02 : MOSInst<0x1F, "mul", ZeroPageX>;
def MUL_Absolute_EL02  : MOSInst<0x2F, "mul", Absolute>;
}

let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def MUL_AbsoluteX_EL02 : MOSInst<0x3F, "mul", AbsoluteX>;
}

// DIV - affects N, Z, V (overflow if divide by zero), C
let Cycles6502 = 5, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def DIV_ZeroPage_EL02  : MOSInst<0x4F, "div", ZeroPage>;
}

let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def DIV_ZeroPageX_EL02 : MOSInst<0x5F, "div", ZeroPageX>;
def DIV_Absolute_EL02  : MOSInst<0x6F, "div", Absolute>;
}

let Cycles6502 = 7, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def DIV_AbsoluteX_EL02 : MOSInst<0x7F, "div", AbsoluteX>;
}

// REA/REI - Push effective: no flags
let Cycles6502 = 5 in {
defm REA           : Op<0x44, "rea", Absolute>;
}

let Cycles6502 = 6 in {
defm REI           : Op<0x54, "rei", Indirect>;
}

// PER - Relative (8-bit): no flags
let Cycles6502 = 4 in {
def PER_Relative   : MOSInst<0x62, "per", Relative>;
}

// RER - Relative error return: no flags
let Cycles6502 = 4 in {
def RER_Relative   : MOSInst<0x82, "rer", Relative>;
}

// PLD - Pull Direct: affects N, Z
let Cycles6502 = 5, FlagN = 1, FlagZ = 1 in {
def PLD_Implied_EL02 : MOSInst<0xCF, "pld", Implicit>;
}

// PHD - Push Direct: no flags
let Cycles6502 = 4 in {
def PHD_Implied_EL02 : MOSInst<0xDF, "phd", Implicit>;
}

// MMU - Memory management: no flags
let Cycles6502 = 2 in {
defm MMU           : Op<0xEF, "mmu", Immediate>;
}

} // DecoderNamespace = "65el02"
} // Predicates = [Has65EL02]
