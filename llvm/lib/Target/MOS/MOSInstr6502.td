//===-- MOSInstr6502.td - MOS 6502 Base Instructions -------*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file describes the base 6502 instructions in TableGen format.
// These are the 56 official opcodes present on all 6502-family processors.
//
//===----------------------------------------------------------------------===//

let Predicates = [Has6502] in {

//===----------------------------------------------------------------------===//
// Special Instructions
//===----------------------------------------------------------------------===//

// BRK sets B flag and pushes PC+2 and P to stack
let Cycles6502 = 7, FlagB = 2, FlagI = 2 in  // Sets B and I flags
def BRK_Implied    : MOSInst<0x00, "brk", Implicit>, TraitUnconditionalBranch;

//===----------------------------------------------------------------------===//
// Logic Instructions (ORA, AND, EOR) - Affect N, Z
//===----------------------------------------------------------------------===//

// Immediate mode: 2 cycles
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
def ORA_Immediate  : MOSInst<0x09, "ora", Immediate> { let MHigh = 1; }
def AND_Immediate  : MOSInst<0x29, "and", Immediate> { let MHigh = 1; }
def EOR_Immediate  : MOSInst<0x49, "eor", Immediate> { let MHigh = 1; }
}

// Zero Page: 3 cycles
let Cycles6502 = 3, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x05, "ora", ZeroPage>;
defm AND           : Op<0x25, "and", ZeroPage>;
defm EOR           : Op<0x45, "eor", ZeroPage>;
}

// Zero Page,X: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x15, "ora", ZeroPageX>;
defm AND           : Op<0x35, "and", ZeroPageX>;
defm EOR           : Op<0x55, "eor", ZeroPageX>;
}

// Absolute: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x0D, "ora", Absolute>;
defm AND           : Op<0x2D, "and", Absolute>;
defm EOR           : Op<0x4D, "eor", Absolute>;
}

// Absolute,X/Y: 4 cycles (+1 on page cross)
let Cycles6502 = 4, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x1D, "ora", AbsoluteX>;
defm ORA           : Op<0x19, "ora", AbsoluteY>;
defm AND           : Op<0x3D, "and", AbsoluteX>;
defm AND           : Op<0x39, "and", AbsoluteY>;
defm EOR           : Op<0x5D, "eor", AbsoluteX>;
defm EOR           : Op<0x59, "eor", AbsoluteY>;
}

// Indexed Indirect (d,X): 6 cycles
let Cycles6502 = 6, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x01, "ora", IndexedIndirect>;
defm AND           : Op<0x21, "and", IndexedIndirect>;
defm EOR           : Op<0x41, "eor", IndexedIndirect>;
}

// Indirect Indexed (d),Y: 5 cycles (+1 on page cross)
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm ORA           : Op<0x11, "ora", IndirectIndexed>;
defm AND           : Op<0x31, "and", IndirectIndexed>;
defm EOR           : Op<0x51, "eor", IndirectIndexed>;
}

//===----------------------------------------------------------------------===//
// Arithmetic Instructions (ADC, SBC) - Affect N, V, Z, C
//===----------------------------------------------------------------------===//

// Immediate: 2 cycles
let Cycles6502 = 2, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
def ADC_Immediate  : MOSInst<0x69, "adc", Immediate> { let MHigh = 1; }
def SBC_Immediate  : MOSInst<0xE9, "sbc", Immediate> { let MHigh = 1; }
}

// Zero Page: 3 cycles
let Cycles6502 = 3, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x65, "adc", ZeroPage>;
defm SBC           : Op<0xE5, "sbc", ZeroPage>;
}

// Zero Page,X: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x75, "adc", ZeroPageX>;
defm SBC           : Op<0xF5, "sbc", ZeroPageX>;
}

// Absolute: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x6D, "adc", Absolute>;
defm SBC           : Op<0xED, "sbc", Absolute>;
}

// Absolute,X/Y: 4 cycles (+1 on page cross)
let Cycles6502 = 4, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x7D, "adc", AbsoluteX>;
defm ADC           : Op<0x79, "adc", AbsoluteY>;
defm SBC           : Op<0xFD, "sbc", AbsoluteX>;
defm SBC           : Op<0xF9, "sbc", AbsoluteY>;
}

// Indexed Indirect (d,X): 6 cycles
let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x61, "adc", IndexedIndirect>;
defm SBC           : Op<0xE1, "sbc", IndexedIndirect>;
}

// Indirect Indexed (d),Y: 5 cycles (+1 on page cross)
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1 in {
defm ADC           : Op<0x71, "adc", IndirectIndexed>;
defm SBC           : Op<0xF1, "sbc", IndirectIndexed>;
}

//===----------------------------------------------------------------------===//
// Compare Instructions (CMP, CPX, CPY) - Affect N, Z, C
//===----------------------------------------------------------------------===//

// Immediate: 2 cycles
let Cycles6502 = 2, FlagN = 1, FlagZ = 1, FlagC = 1 in {
def CMP_Immediate  : MOSInst<0xC9, "cmp", Immediate> { let MHigh = 1; }
def CPY_Immediate  : MOSInst<0xC0, "cpy", Immediate> { let XHigh = 1; }
def CPX_Immediate  : MOSInst<0xE0, "cpx", Immediate> { let XHigh = 1; }
}

// Zero Page: 3 cycles
let Cycles6502 = 3, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CPY           : Op<0xC4, "cpy", ZeroPage>;
defm CMP           : Op<0xC5, "cmp", ZeroPage>;
defm CPX           : Op<0xE4, "cpx", ZeroPage>;
}

// Zero Page,X: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xD5, "cmp", ZeroPageX>;
}

// Absolute: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CPY           : Op<0xCC, "cpy", Absolute>;
defm CMP           : Op<0xCD, "cmp", Absolute>;
defm CPX           : Op<0xEC, "cpx", Absolute>;
}

// Absolute,X/Y: 4 cycles (+1 on page cross)
let Cycles6502 = 4, PageCrossCycles = 1, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xDD, "cmp", AbsoluteX>;
defm CMP           : Op<0xD9, "cmp", AbsoluteY>;
}

// Indexed Indirect (d,X): 6 cycles
let Cycles6502 = 6, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xC1, "cmp", IndexedIndirect>;
}

// Indirect Indexed (d),Y: 5 cycles (+1 on page cross)
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm CMP           : Op<0xD1, "cmp", IndirectIndexed>;
}

//===----------------------------------------------------------------------===//
// BIT Instruction - Affects N, V, Z
//===----------------------------------------------------------------------===//

// Zero Page: 3 cycles
let Cycles6502 = 3, FlagN = 1, FlagV = 1, FlagZ = 1 in {
defm BIT           : Op<0x24, "bit", ZeroPage>;
}

// Absolute: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagZ = 1 in {
defm BIT           : Op<0x2C, "bit", Absolute>;
}

//===----------------------------------------------------------------------===//
// Load Instructions (LDA, LDX, LDY) - Affect N, Z
//===----------------------------------------------------------------------===//

// Immediate: 2 cycles
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
def LDY_Immediate  : MOSInst<0xA0, "ldy", Immediate> { let XHigh = 1; }
def LDX_Immediate  : MOSInst<0xA2, "ldx", Immediate> { let XHigh = 1; }
def LDA_Immediate  : MOSInst<0xA9, "lda", Immediate> { let MHigh = 1; }
}

// Zero Page: 3 cycles
let Cycles6502 = 3, FlagN = 1, FlagZ = 1 in {
defm LDY           : Op<0xA4, "ldy", ZeroPage>;
defm LDA           : Op<0xA5, "lda", ZeroPage>;
defm LDX           : Op<0xA6, "ldx", ZeroPage>;
}

// Zero Page,X/Y: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm LDY           : Op<0xB4, "ldy", ZeroPageX>;
defm LDA           : Op<0xB5, "lda", ZeroPageX>;
defm LDX           : Op<0xB6, "ldx", ZeroPageY>;
}

// Absolute: 4 cycles
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm LDY           : Op<0xAC, "ldy", Absolute>;
defm LDA           : Op<0xAD, "lda", Absolute>;
defm LDX           : Op<0xAE, "ldx", Absolute>;
}

// Absolute,X/Y: 4 cycles (+1 on page cross)
let Cycles6502 = 4, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm LDY           : Op<0xBC, "ldy", AbsoluteX>;
defm LDA           : Op<0xBD, "lda", AbsoluteX>;
defm LDA           : Op<0xB9, "lda", AbsoluteY>;
defm LDX           : Op<0xBE, "ldx", AbsoluteY>;
}

// Indexed Indirect (d,X): 6 cycles
let Cycles6502 = 6, FlagN = 1, FlagZ = 1 in {
defm LDA           : Op<0xA1, "lda", IndexedIndirect>;
}

// Indirect Indexed (d),Y: 5 cycles (+1 on page cross)
let Cycles6502 = 5, PageCrossCycles = 1, FlagN = 1, FlagZ = 1 in {
defm LDA           : Op<0xB1, "lda", IndirectIndexed>;
}

//===----------------------------------------------------------------------===//
// Store Instructions (STA, STX, STY) - No flags affected
//===----------------------------------------------------------------------===//

// Zero Page: 3 cycles
let Cycles6502 = 3 in {
defm STY           : Op<0x84, "sty", ZeroPage>;
defm STA           : Op<0x85, "sta", ZeroPage>;
defm STX           : Op<0x86, "stx", ZeroPage>;
}

// Zero Page,X/Y: 4 cycles
let Cycles6502 = 4 in {
defm STY           : Op<0x94, "sty", ZeroPageX>;
defm STA           : Op<0x95, "sta", ZeroPageX>;
defm STX           : Op<0x96, "stx", ZeroPageY>;
}

// Absolute: 4 cycles
let Cycles6502 = 4 in {
defm STY           : Op<0x8C, "sty", Absolute>;
defm STA           : Op<0x8D, "sta", Absolute>;
defm STX           : Op<0x8E, "stx", Absolute>;
}

// Absolute,X/Y: 5 cycles (no page cross penalty)
let Cycles6502 = 5 in {
defm STA           : Op<0x9D, "sta", AbsoluteX>;
defm STA           : Op<0x99, "sta", AbsoluteY>;
}

// Indexed Indirect (d,X): 6 cycles
let Cycles6502 = 6 in {
defm STA           : Op<0x81, "sta", IndexedIndirect>;
}

// Indirect Indexed (d),Y: 6 cycles (no page cross penalty for stores)
let Cycles6502 = 6 in {
defm STA           : Op<0x91, "sta", IndirectIndexed>;
}

//===----------------------------------------------------------------------===//
// Shift/Rotate Instructions (ASL, LSR, ROL, ROR) - Affect N, Z, C
//===----------------------------------------------------------------------===//

// Accumulator: 2 cycles
let Cycles6502 = 2, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASL           : Op<0x0A, "asl", Accumulator>;
defm ROL           : Op<0x2A, "rol", Accumulator>;
defm LSR           : Op<0x4A, "lsr", Accumulator>;
defm ROR           : Op<0x6A, "ror", Accumulator>;
}

// Zero Page: 5 cycles
let Cycles6502 = 5, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASL           : Op<0x06, "asl", ZeroPage>;
defm ROL           : Op<0x26, "rol", ZeroPage>;
defm LSR           : Op<0x46, "lsr", ZeroPage>;
defm ROR           : Op<0x66, "ror", ZeroPage>;
}

// Zero Page,X: 6 cycles
let Cycles6502 = 6, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASL           : Op<0x16, "asl", ZeroPageX>;
defm ROL           : Op<0x36, "rol", ZeroPageX>;
defm LSR           : Op<0x56, "lsr", ZeroPageX>;
defm ROR           : Op<0x76, "ror", ZeroPageX>;
}

// Absolute: 6 cycles
let Cycles6502 = 6, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASL           : Op<0x0E, "asl", Absolute>;
defm ROL           : Op<0x2E, "rol", Absolute>;
defm LSR           : Op<0x4E, "lsr", Absolute>;
defm ROR           : Op<0x6E, "ror", Absolute>;
}

// Absolute,X: 7 cycles
let Cycles6502 = 7, FlagN = 1, FlagZ = 1, FlagC = 1 in {
defm ASL           : Op<0x1E, "asl", AbsoluteX>;
defm ROL           : Op<0x3E, "rol", AbsoluteX>;
defm LSR           : Op<0x5E, "lsr", AbsoluteX>;
defm ROR           : Op<0x7E, "ror", AbsoluteX>;
}

//===----------------------------------------------------------------------===//
// Increment/Decrement Instructions (INC, DEC, INX, DEX, INY, DEY) - Affect N, Z
//===----------------------------------------------------------------------===//

// Memory operations: 5-7 cycles depending on addressing mode
let Cycles6502 = 5, FlagN = 1, FlagZ = 1 in {
defm DEC           : Op<0xC6, "dec", ZeroPage>;
defm INC           : Op<0xE6, "inc", ZeroPage>;
}

let Cycles6502 = 6, FlagN = 1, FlagZ = 1 in {
defm DEC           : Op<0xD6, "dec", ZeroPageX>;
defm INC           : Op<0xF6, "inc", ZeroPageX>;
defm DEC           : Op<0xCE, "dec", Absolute>;
defm INC           : Op<0xEE, "inc", Absolute>;
}

let Cycles6502 = 7, FlagN = 1, FlagZ = 1 in {
defm DEC           : Op<0xDE, "dec", AbsoluteX>;
defm INC           : Op<0xFE, "inc", AbsoluteX>;
}

// Register operations: 2 cycles
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm DEY           : Op<0x88, "dey", Implicit>;
defm INY           : Op<0xC8, "iny", Implicit>;
defm DEX           : Op<0xCA, "dex", Implicit>;
defm INX           : Op<0xE8, "inx", Implicit>;
}

//===----------------------------------------------------------------------===//
// Transfer Instructions - Affect N, Z (except TXS)
//===----------------------------------------------------------------------===//

let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
defm TAX           : Op<0xAA, "tax", Implicit>;
defm TAY           : Op<0xA8, "tay", Implicit>;
defm TXA           : Op<0x8A, "txa", Implicit>;
defm TYA           : Op<0x98, "tya", Implicit>;
defm TSX           : Op<0xBA, "tsx", Implicit>;
}

// TXS doesn't affect any flags
let Cycles6502 = 2 in {
defm TXS           : Op<0x9A, "txs", Implicit>;
}

//===----------------------------------------------------------------------===//
// Flag Instructions - Affect specific flags
//===----------------------------------------------------------------------===//

let Cycles6502 = 2 in {
// Clear flags (set to 0)
let FlagC = 3 in defm CLC : Op<0x18, "clc", Implicit>;  // Clear Carry
let FlagI = 3 in defm CLI : Op<0x58, "cli", Implicit>;  // Clear Interrupt Disable
let FlagV = 3 in defm CLV : Op<0xB8, "clv", Implicit>;  // Clear Overflow
let FlagD = 3 in defm CLD : Op<0xD8, "cld", Implicit>;  // Clear Decimal

// Set flags (set to 1)
let FlagC = 2 in defm SEC : Op<0x38, "sec", Implicit>;  // Set Carry
let FlagI = 2 in defm SEI : Op<0x78, "sei", Implicit>;  // Set Interrupt Disable
let FlagD = 2 in defm SED : Op<0xF8, "sed", Implicit>;  // Set Decimal
}

//===----------------------------------------------------------------------===//
// Stack Operations
//===----------------------------------------------------------------------===//

// Push: 3 cycles, no flag effects
let Cycles6502 = 3 in {
defm PHP           : Op<0x08, "php", Implicit>;
defm PHA           : Op<0x48, "pha", Implicit>;
}

// PLA: 4 cycles, affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1 in {
defm PLA           : Op<0x68, "pla", Implicit>;
}

// PLP: 4 cycles, restores all flags
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagB = 1, FlagD = 1, FlagI = 1, FlagZ = 1, FlagC = 1 in {
defm PLP           : Op<0x28, "plp", Implicit>;
}

//===----------------------------------------------------------------------===//
// No-Operation
//===----------------------------------------------------------------------===//

let Cycles6502 = 2 in {
defm NOP           : Op<0xEA, "nop", Implicit>;
}

//===----------------------------------------------------------------------===//
// Branch Instructions - No flag effects
//===----------------------------------------------------------------------===//

let Cycles6502 = 2, PageCrossCycles = 1 in {
defm BPL           : OpBranch<0x10, "bpl">;
defm BMI           : OpBranch<0x30, "bmi">;
defm BVC           : OpBranch<0x50, "bvc">;
defm BVS           : OpBranch<0x70, "bvs">;
defm BCC           : OpBranch<0x90, "bcc">;
defm BCS           : OpBranch<0xB0, "bcs">;
defm BNE           : OpBranch<0xD0, "bne">;
defm BEQ           : OpBranch<0xF0, "beq">;
}

//===----------------------------------------------------------------------===//
// Jump/Call/Return Instructions
//===----------------------------------------------------------------------===//

// JMP: No flag effects
let Cycles6502 = 3 in
defm JMP           : OpJump<0x4C, "jmp", Absolute>;

let Cycles6502 = 5 in
defm JMP           : OpJumpIndirect<0x6C, "jmp", Indirect16>;

// JSR/RTS: No flag effects
let Cycles6502 = 6 in {
defm JSR           : OpCall<0x20, "jsr", Absolute>;
defm RTS           : OpReturn<0x60, "rts">;
}

// RTI: Restores all flags from stack
let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagB = 1, FlagD = 1, FlagI = 1, FlagZ = 1, FlagC = 1 in {
defm RTI           : OpReturn<0x40, "rti">;
}

//===----------------------------------------------------------------------===//
// Aliases
//===----------------------------------------------------------------------===//

def : InstAlias<"asl a", (ASL_Accumulator)>;
def : InstAlias<"rol a", (ROL_Accumulator)>;
def : InstAlias<"lsr a", (LSR_Accumulator)>;
def : InstAlias<"ror a", (ROR_Accumulator)>;

} // Predicates = [Has6502]
