//===-- MOSInstr65C02.td - MOS 65C02 Family Instructions ---*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes 65C02 family instructions in TableGen format.
// Includes 65C02, R65C02 (Rockwell), and W65C02 (WDC) variants.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// 65C02 Instructions
//===----------------------------------------------------------------------===//

let Predicates = [Has65C02] in {

// INC/DEC Accumulator - affect N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm INC           : Op<0x1A, "inc", Accumulator>;
defm DEC           : Op<0x3A, "dec", Accumulator>;
}

// BIT Immediate - affects Z only (not N, V like memory-addressed BIT)
let Cycles6502 = 2, FlagZ = 1 in {
def BIT_Immediate  : MOSInst<0x89, "bit", Immediate> { let MHigh = 1; }
}

// Stack push - no flags affected
let Cycles6502 = 3 in {
defm PHY           : Op<0x5A, "phy", Implicit>;
defm PHX           : Op<0xDA, "phx", Implicit>;
}

// BRA - unconditional relative branch, no flags
let Cycles6502 = 3, PageCrossCycles = 1 in {
defm BRA           : OpJump<0x80, "bra", Relative>;
}

// Stack pull - affect N, Z from pulled value
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm PLY           : Op<0x7A, "ply", Implicit>;
defm PLX           : Op<0xFA, "plx", Implicit>;
}

// STZ - store zero, no flags affected
let Cycles6502 = 3 in {
defm STZ           : Op<0x64, "stz", ZeroPage>;
}

let Cycles6502 = 4 in {
defm STZ           : Op<0x74, "stz", ZeroPageX>;
defm STZ           : Op<0x9C, "stz", Absolute>;
}

let Cycles6502 = 5 in {
defm STZ           : Op<0x9E, "stz", AbsoluteX>;
}

// TSB/TRB - Test and Set/Reset Bits - affect Z only
let Cycles6502 = 5, FlagZ = 1 in {
defm TSB           : Op<0x04, "tsb", ZeroPage>;
defm TRB           : Op<0x14, "trb", ZeroPage>;
}

let Cycles6502 = 6, FlagZ = 1 in {
defm TSB           : Op<0x0C, "tsb", Absolute>;
defm TRB           : Op<0x1C, "trb", Absolute>;
}

// Logic operations (Indirect mode) - affect N, Z
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x12, "ora", Indirect>;
defm AND           : Op<0x32, "and", Indirect>;
defm EOR           : Op<0x52, "eor", Indirect>;
defm LDA           : Op<0xB2, "lda", Indirect>;
}

// Arithmetic (Indirect mode) - affect N, V, Z, C
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x72, "adc", Indirect>;
defm SBC           : Op<0xF2, "sbc", Indirect>;
}

// Compare (Indirect mode) - affect N, Z, C
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xD2, "cmp", Indirect>;
}

// STA - store, no flags
let Cycles6502 = 5 in {
defm STA           : Op<0x92, "sta", Indirect>;
}

// BIT with memory - affects N, V, Z (N=bit7, V=bit6, Z=A AND mem)
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagZ = 1 in {
defm BIT           : Op<0x34, "bit", ZeroPageX>;
}

let Cycles6502 = 4, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1 in {
defm BIT           : Op<0x3C, "bit", AbsoluteX>;
}

// JMP indexed indirect - no flags
let Cycles6502 = 6 in {
defm JMP           : OpJumpIndirect<0x7C, "jmp", IndexedIndirect16>;
}

def : InstAlias<"inc a", (INC_Accumulator)>;
def : InstAlias<"dec a", (DEC_Accumulator)>;
def : InstAlias<"ina", (INC_Accumulator), 0>;
def : InstAlias<"dea", (DEC_Accumulator), 0>;

} // Predicates = [Has65C02]

//===----------------------------------------------------------------------===//
// R65C02 Instructions (Rockwell - Bit manipulation)
//===----------------------------------------------------------------------===//

let Predicates = [HasR65C02] in {
let DecoderNamespace = "r65c02" in {

// RMB/SMB - Reset/Set Memory Bit - no flags affected
let Cycles6502 = 5 in {
def RMB_ZeroPage   : InstBitModify<"rmb", Opcode<0x07>, ZeroPage>;
def SMB_ZeroPage   : InstBitModify<"smb", Opcode<0x87>, ZeroPage>;
}

// BBR/BBS - Branch on Bit Reset/Set - no flags affected
let Cycles6502 = 5, PageCrossCycles = 1 in {
def BBR_ZeroPage   : InstBitBranch<"bbr", Opcode<0x0F>>;
def BBS_ZeroPage   : InstBitBranch<"bbs", Opcode<0x8F>>;
}

} // DecoderNamespace = "r65c02"

def BBR0_ZeroPage  : InstAlias<"bbr0 $source, $location", (BBR_ZeroPage 0, addr8:$source, pcrel8at2:$location)>;
def BBR1_ZeroPage  : InstAlias<"bbr1 $source, $location", (BBR_ZeroPage 1, addr8:$source, pcrel8at2:$location)>;
def BBR2_ZeroPage  : InstAlias<"bbr2 $source, $location", (BBR_ZeroPage 2, addr8:$source, pcrel8at2:$location)>;
def BBR3_ZeroPage  : InstAlias<"bbr3 $source, $location", (BBR_ZeroPage 3, addr8:$source, pcrel8at2:$location)>;
def BBR4_ZeroPage  : InstAlias<"bbr4 $source, $location", (BBR_ZeroPage 4, addr8:$source, pcrel8at2:$location)>;
def BBR5_ZeroPage  : InstAlias<"bbr5 $source, $location", (BBR_ZeroPage 5, addr8:$source, pcrel8at2:$location)>;
def BBR6_ZeroPage  : InstAlias<"bbr6 $source, $location", (BBR_ZeroPage 6, addr8:$source, pcrel8at2:$location)>;
def BBR7_ZeroPage  : InstAlias<"bbr7 $source, $location", (BBR_ZeroPage 7, addr8:$source, pcrel8at2:$location)>;

def BBS0_ZeroPage  : InstAlias<"bbs0 $source, $location", (BBS_ZeroPage 0, addr8:$source, pcrel8at2:$location)>;
def BBS1_ZeroPage  : InstAlias<"bbs1 $source, $location", (BBS_ZeroPage 1, addr8:$source, pcrel8at2:$location)>;
def BBS2_ZeroPage  : InstAlias<"bbs2 $source, $location", (BBS_ZeroPage 2, addr8:$source, pcrel8at2:$location)>;
def BBS3_ZeroPage  : InstAlias<"bbs3 $source, $location", (BBS_ZeroPage 3, addr8:$source, pcrel8at2:$location)>;
def BBS4_ZeroPage  : InstAlias<"bbs4 $source, $location", (BBS_ZeroPage 4, addr8:$source, pcrel8at2:$location)>;
def BBS5_ZeroPage  : InstAlias<"bbs5 $source, $location", (BBS_ZeroPage 5, addr8:$source, pcrel8at2:$location)>;
def BBS6_ZeroPage  : InstAlias<"bbs6 $source, $location", (BBS_ZeroPage 6, addr8:$source, pcrel8at2:$location)>;
def BBS7_ZeroPage  : InstAlias<"bbs7 $source, $location", (BBS_ZeroPage 7, addr8:$source, pcrel8at2:$location)>;

def RMB0_ZeroPage  : InstAlias<"rmb0 $source", (RMB_ZeroPage 0, addr8:$source)>;
def RMB1_ZeroPage  : InstAlias<"rmb1 $source", (RMB_ZeroPage 1, addr8:$source)>;
def RMB2_ZeroPage  : InstAlias<"rmb2 $source", (RMB_ZeroPage 2, addr8:$source)>;
def RMB3_ZeroPage  : InstAlias<"rmb3 $source", (RMB_ZeroPage 3, addr8:$source)>;
def RMB4_ZeroPage  : InstAlias<"rmb4 $source", (RMB_ZeroPage 4, addr8:$source)>;
def RMB5_ZeroPage  : InstAlias<"rmb5 $source", (RMB_ZeroPage 5, addr8:$source)>;
def RMB6_ZeroPage  : InstAlias<"rmb6 $source", (RMB_ZeroPage 6, addr8:$source)>;
def RMB7_ZeroPage  : InstAlias<"rmb7 $source", (RMB_ZeroPage 7, addr8:$source)>;

def SMB0_ZeroPage  : InstAlias<"smb0 $source", (SMB_ZeroPage 0, addr8:$source)>;
def SMB1_ZeroPage  : InstAlias<"smb1 $source", (SMB_ZeroPage 1, addr8:$source)>;
def SMB2_ZeroPage  : InstAlias<"smb2 $source", (SMB_ZeroPage 2, addr8:$source)>;
def SMB3_ZeroPage  : InstAlias<"smb3 $source", (SMB_ZeroPage 3, addr8:$source)>;
def SMB4_ZeroPage  : InstAlias<"smb4 $source", (SMB_ZeroPage 4, addr8:$source)>;
def SMB5_ZeroPage  : InstAlias<"smb5 $source", (SMB_ZeroPage 5, addr8:$source)>;
def SMB6_ZeroPage  : InstAlias<"smb6 $source", (SMB_ZeroPage 6, addr8:$source)>;
def SMB7_ZeroPage  : InstAlias<"smb7 $source", (SMB_ZeroPage 7, addr8:$source)>;

} // Predicates = [HasR65C02]

//===----------------------------------------------------------------------===//
// W65C02 Instructions (WDC)
//===----------------------------------------------------------------------===//

let Predicates = [HasW65C02] in {

// WAI, STP - no flags affected (halts CPU until interrupt/reset)
let Cycles6502 = 3 in {
defm WAI           : Op<0xCB, "wai", Implicit>;
defm STP           : Op<0xDB, "stp", Implicit>;
}

} // Predicates = [HasW65C02]
