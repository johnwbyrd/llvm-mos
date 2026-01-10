//===-- MOSInstrHuC6280.td - MOS HuC6280 Instructions ------*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes Hudson/NEC HuC6280 (PC Engine/TurboGrafx-16)
// instructions in TableGen format.
//
//===----------------------------------------------------------------------===//

let Predicates = [HasHUC6280] in {
let DecoderNamespace = "huc6280" in {

// CLA/CLX/CLY - Clear register: affects N, Z (always sets Z, clears N)
// Note: These are special - result is always 0, so Z=1, N=0
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm CLA           : Op<0x62, "cla", Implicit>;
defm CLX           : Op<0x82, "clx", Implicit>;
defm CLY           : Op<0xC2, "cly", Implicit>;
}

// SXY/SAX/SAY - Swap registers: no flags affected
let Cycles6502 = 3 in {
defm SXY           : Op<0x02, "sxy", Implicit>;
defm SAX           : Op<0x22, "sax", Implicit>;
defm SAY           : Op<0x42, "say", Implicit>;
}

// ST0/ST1/ST2 - Store to VDC: no flags affected
let Cycles6502 = 4 in {
defm ST0           : Op<0x03, "st0", Immediate>;
defm ST1           : Op<0x13, "st1", Immediate>;
defm ST2           : Op<0x23, "st2", Immediate>;
}

// TAM/TMA - Memory mapping registers: no flags affected
let Cycles6502 = 4 in {
defm TMA           : Op<0x43, "tma", Immediate>;
defm TAM           : Op<0x53, "tam", Immediate>;
}

// CSL/CSH - Clock speed control: no flags affected
let Cycles6502 = 3 in {
defm CSL           : Op<0x54, "csl", Implicit>;
defm CSH           : Op<0xD4, "csh", Implicit>;
}

// SET - Set T flag for decimal mode: affects D (sets it)
let Cycles6502 = 2, FlagD = 2 in {
defm SET           : Op<0xF4, "set", Implicit>;
}

// BSR - Branch to subroutine (relative): no flags affected
let Cycles6502 = 8 in {
defm BSR           : OpCall<0x44, "bsr", Relative>;
}

// TST - Test and set bits: affects N, V, Z
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagZ = 1 in {
def TST_ZeroPage   : InstImmediate24<"tst", Opcode<0x83>, ZeroPageAt2>;
}

let Cycles6502 = 5, FlagN = 1, FlagV = 1, FlagZ = 1 in {
def TST_Absolute   : InstImmediate32<"tst", Opcode<0x93>, AbsoluteAt2>;
def TST_ZeroPageX  : InstImmediate24<"tst", Opcode<0xA3>, ZeroPageXAt2>;
}

let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagZ = 1 in {
def TST_AbsoluteX  : InstImmediate32<"tst", Opcode<0xB3>, AbsoluteXAt2>;
}

// Block transfer instructions - no flags affected
// Cycles depend on length (17 + 6*count cycles), using nominal 17 base
let Cycles6502 = 17 in {
def TII_HuCBlockMove : InstHuCBlockMove<"tii", Opcode<0x73>>;
def TDD_HuCBlockMove : InstHuCBlockMove<"tdd", Opcode<0xC3>>;
def TIN_HuCBlockMove : InstHuCBlockMove<"tin", Opcode<0xD3>>;
def TIA_HuCBlockMove : InstHuCBlockMove<"tia", Opcode<0xE3>>;
def TAI_HuCBlockMove : InstHuCBlockMove<"tai", Opcode<0xF3>>;
}

} // DecoderNamespace = "huc6280"
} // Predicates = [HasHUC6280]
