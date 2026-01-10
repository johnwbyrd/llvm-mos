//===-- MOSInstr6502X.td - MOS 6502 Illegal Opcodes --------*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the undocumented/illegal 6502 opcodes and 65DTV02
// instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// 6502 Undocumented/Illegal Opcodes - Common with DTV
//===----------------------------------------------------------------------===//

let Predicates = [Has6502XOrDTV] in {
let DecoderNamespace = "6502x" in {

// ALR (ASR) - AND + LSR: affects N, Z, C
let Cycles6502 = 2, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ALR           : Op<0x4B, "alr", Immediate>;
}

// ARR - AND + ROR with special flag behavior: affects N, V, Z, C
// (C from bit 6 of result, V from bit 6 XOR bit 5)
let Cycles6502 = 2, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ARR           : Op<0x6B, "arr", Immediate>;
}

// ANE/XAA - unstable, AND with magic constant: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm ANE           : Op<0x8B, "ane", Immediate>;
}

// LAX - LDA + LDX combined: affects N, Z
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm LAX           : Op<0xAB, "lax", Immediate>;
}

// LAX - Zero Page: affects N, Z
let Cycles6502 = 3, FlagN = 1, FlagZ = 1 in {
defm LAX           : Op<0xA7, "lax", ZeroPage>;
}

// SAX - store A AND X: no flags affected
let Cycles6502 = 3 in {
defm SAX           : Op<0x87, "sax", ZeroPage>;
}

// SAX - Zero Page,Y: no flags
let Cycles6502 = 4 in {
defm SAX           : Op<0x97, "sax", ZeroPageY>;
}

// LAX - Zero Page,Y: affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm LAX           : Op<0xB7, "lax", ZeroPageY>;
}

// LAX - Absolute: affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm LAX           : Op<0xAF, "lax", Absolute>;
}

// SAX - Absolute: no flags
let Cycles6502 = 4 in {
defm SAX           : Op<0x8F, "sax", Absolute>;
}

// LAX - Absolute,Y: affects N, Z
let Cycles6502 = 4, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm LAX           : Op<0xBF, "lax", AbsoluteY>;
}

// SLO - ASL + ORA: affects N, Z, C
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm SLO           : Op<0x07, "slo", ZeroPage>;
}

// RLA - ROL + AND: affects N, Z, C
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm RLA           : Op<0x27, "rla", ZeroPage>;
}

// SRE - LSR + EOR: affects N, Z, C
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm SRE           : Op<0x47, "sre", ZeroPage>;
}

// RRA - ROR + ADC: affects N, V, Z, C
let Cycles6502 = 5, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm RRA           : Op<0x67, "rra", ZeroPage>;
}

// DCP - DEC + CMP: affects N, Z, C
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm DCP           : Op<0xC7, "dcp", ZeroPage>;
}

// ISC/ISB - INC + SBC: affects N, V, Z, C
let Cycles6502 = 5, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ISC           : Op<0xE7, "isc", ZeroPage>;
}

// LAX - Indirect Indexed: affects N, Z
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm LAX           : Op<0xB3, "lax", IndirectIndexed>;
}

// Zero Page,X RMW operations
let Cycles6502 = 6, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm SLO           : Op<0x17, "slo", ZeroPageX>;
defm RLA           : Op<0x37, "rla", ZeroPageX>;
defm SRE           : Op<0x57, "sre", ZeroPageX>;
defm DCP           : Op<0xD7, "dcp", ZeroPageX>;
}

let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm RRA           : Op<0x77, "rra", ZeroPageX>;
defm ISC           : Op<0xF7, "isc", ZeroPageX>;
}

// Absolute RMW operations
let Cycles6502 = 6, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm SLO           : Op<0x0F, "slo", Absolute>;
defm RLA           : Op<0x2F, "rla", Absolute>;
defm SRE           : Op<0x4F, "sre", Absolute>;
defm DCP           : Op<0xCF, "dcp", Absolute>;
}

let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm RRA           : Op<0x6F, "rra", Absolute>;
defm ISC           : Op<0xEF, "isc", Absolute>;
}

// LAX - Indexed Indirect: affects N, Z
let Cycles6502 = 6, FlagN = 1, FlagZ = 1 in {
defm LAX           : Op<0xA3, "lax", IndexedIndirect>;
}

// SAX - Indexed Indirect: no flags
let Cycles6502 = 6 in {
defm SAX           : Op<0x83, "sax", IndexedIndirect>;
}

// Absolute,X/Y RMW operations
let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm SLO           : Op<0x1F, "slo", AbsoluteX>;
defm SLO           : Op<0x1B, "slo", AbsoluteY>;
defm RLA           : Op<0x3F, "rla", AbsoluteX>;
defm RLA           : Op<0x3B, "rla", AbsoluteY>;
defm SRE           : Op<0x5F, "sre", AbsoluteX>;
defm SRE           : Op<0x5B, "sre", AbsoluteY>;
defm DCP           : Op<0xDF, "dcp", AbsoluteX>;
defm DCP           : Op<0xDB, "dcp", AbsoluteY>;
}

let Cycles6502 = 7, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm RRA           : Op<0x7F, "rra", AbsoluteX>;
defm RRA           : Op<0x7B, "rra", AbsoluteY>;
defm ISC           : Op<0xFF, "isc", AbsoluteX>;
defm ISC           : Op<0xFB, "isc", AbsoluteY>;
}

// Indexed Indirect (d,X) RMW operations
let Cycles6502 = 8, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm SLO           : Op<0x03, "slo", IndexedIndirect>;
defm RLA           : Op<0x23, "rla", IndexedIndirect>;
defm SRE           : Op<0x43, "sre", IndexedIndirect>;
defm DCP           : Op<0xC3, "dcp", IndexedIndirect>;
}

let Cycles6502 = 8, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm RRA           : Op<0x63, "rra", IndexedIndirect>;
defm ISC           : Op<0xE3, "isc", IndexedIndirect>;
}

// Indirect Indexed (d),Y RMW operations
let Cycles6502 = 8, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm SLO           : Op<0x13, "slo", IndirectIndexed>;
defm RLA           : Op<0x33, "rla", IndirectIndexed>;
defm SRE           : Op<0x53, "sre", IndirectIndexed>;
defm DCP           : Op<0xD3, "dcp", IndirectIndexed>;
}

let Cycles6502 = 8, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm RRA           : Op<0x73, "rra", IndirectIndexed>;
defm ISC           : Op<0xF3, "isc", IndirectIndexed>;
}

} // DecoderNamespace = "6502x"
} // Predicates = [Has6502XOrDTV]

//===----------------------------------------------------------------------===//
// 6502 Undocumented/Illegal Opcodes (6502X only)
//===----------------------------------------------------------------------===//

let Predicates = [Has6502X] in {
let DecoderNamespace = "6502x" in {

// ANC - AND + copy N to C: affects N, Z, C
let Cycles6502 = 2, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ANC           : Op<0x2B, "anc", Immediate>;
}

// SBX/AXS - (A AND X) - imm: affects N, Z, C
let Cycles6502 = 2, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm SBX           : Op<0xCB, "sbx", Immediate>;
}

// NOP variants - no flags affected
let Cycles6502 = 2 in {
def NOP_Immediate  : MOSInst<0x1A, "nop", Immediate>;
}

let Cycles6502 = 3 in {
def NOP_ZeroPage   : MOSInst<0x04, "nop", ZeroPage>;
}

let Cycles6502 = 4 in {
def NOP_ZeroPageX  : MOSInst<0x14, "nop", ZeroPageX>;
}

let Cycles6502 = 4, PageCrossCycles = 1 in {
def NOP_AbsoluteX  : MOSInst<0x1C, "nop", AbsoluteX>;
}

// LAS - LDA + LDX + TXS: affects N, Z
let Cycles6502 = 4, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm LAS           : Op<0xBB, "las", AbsoluteY>;
}

// Unstable instructions - stores with side effects, no flags
let Cycles6502 = 5 in {
defm SHA           : Op<0x93, "sha", IndirectIndexed>;
defm TAS           : Op<0x9B, "tas", AbsoluteY>;
defm SHY           : Op<0x9C, "shy", AbsoluteX>;
defm SHX           : Op<0x9E, "shx", AbsoluteY>;
def SHA_AbsoluteY  : MOSInst<0x9F, "sha", AbsoluteY>;
}

} // DecoderNamespace = "6502x"
} // Predicates = [Has6502X]

//===----------------------------------------------------------------------===//
// 65DTV02 Instructions
//===----------------------------------------------------------------------===//

let Predicates = [Has65DTV02] in {
let DecoderNamespace = "65dtv02" in {

// BRA - unconditional branch, no flags
let Cycles6502 = 2, PageCrossCycles = 1 in {
def BRA_Relative_DTV02 : MOSInst<0x12, "bra", Relative>, TraitUnconditionalBranch;
}

// SAC/SIR - set accumulator/index source, no flags affected
let Cycles6502 = 2 in {
defm SAC           : Op<0x32, "sac", Immediate>;
defm SIR           : Op<0x42, "sir", Immediate>;
}

} // DecoderNamespace = "65dtv02"
} // Predicates = [Has65DTV02]
