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

// BRK - Break (software interrupt)
// Pushes PC+2 and P (with B flag set), then loads IRQ/BRK vector
// HaltEmulation=1: halts the emulator after execution
let Cycles6502 = 7, FlagB = 2, FlagI = 2, HaltEmulation = 1,
    Emulate = [{
  uint16_t ret = PC + 2;
  push((ret >> 8) & 0xFF);
  push(ret & 0xFF);
  push(getP() | 0x30);  // B and unused flag set
  I = true;
  PC = read(0xFFFE) | (read(0xFFFF) << 8);
  PCModified = true;
}] in
def BRK_Implied    : MOSInst<0x00, "brk", Implicit>, TraitUnconditionalBranch;

//===----------------------------------------------------------------------===//
// Logic Instructions (ORA, AND, EOR) - Affect N, Z
//===----------------------------------------------------------------------===//

// ORA - Logical OR with Accumulator
let FlagN = 1, FlagZ = 1, Emulate = [{ A |= $Value; setNZ(A); }] in {
  let Cycles6502 = 2 in
  def ORA_Immediate  : MOSInst<0x09, "ora", Immediate> { let MHigh = 1; }
  let Cycles6502 = 3 in
  defm ORA           : Op<0x05, "ora", ZeroPage>;
  let Cycles6502 = 4 in
  defm ORA           : Op<0x15, "ora", ZeroPageX>;
  let Cycles6502 = 4 in
  defm ORA           : Op<0x0D, "ora", Absolute>;
  let Cycles6502 = 6 in
  defm ORA           : Op<0x01, "ora", IndexedIndirect>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in {
    defm ORA           : OpPgX<0x1D, "ora", AbsoluteX>;
    defm ORA           : OpPgX<0x19, "ora", AbsoluteY>;
  }
  let Cycles6502 = 5 in
  defm ORA           : OpPgX<0x11, "ora", IndirectIndexed>;
}

// AND - Logical AND with Accumulator
let FlagN = 1, FlagZ = 1, Emulate = [{ A &= $Value; setNZ(A); }] in {
  let Cycles6502 = 2 in
  def AND_Immediate  : MOSInst<0x29, "and", Immediate> { let MHigh = 1; }
  let Cycles6502 = 3 in
  defm AND           : Op<0x25, "and", ZeroPage>;
  let Cycles6502 = 4 in
  defm AND           : Op<0x35, "and", ZeroPageX>;
  let Cycles6502 = 4 in
  defm AND           : Op<0x2D, "and", Absolute>;
  let Cycles6502 = 6 in
  defm AND           : Op<0x21, "and", IndexedIndirect>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in {
    defm AND           : OpPgX<0x3D, "and", AbsoluteX>;
    defm AND           : OpPgX<0x39, "and", AbsoluteY>;
  }
  let Cycles6502 = 5 in
  defm AND           : OpPgX<0x31, "and", IndirectIndexed>;
}

// EOR - Exclusive OR with Accumulator
let FlagN = 1, FlagZ = 1, Emulate = [{ A ^= $Value; setNZ(A); }] in {
  let Cycles6502 = 2 in
  def EOR_Immediate  : MOSInst<0x49, "eor", Immediate> { let MHigh = 1; }
  let Cycles6502 = 3 in
  defm EOR           : Op<0x45, "eor", ZeroPage>;
  let Cycles6502 = 4 in
  defm EOR           : Op<0x55, "eor", ZeroPageX>;
  let Cycles6502 = 4 in
  defm EOR           : Op<0x4D, "eor", Absolute>;
  let Cycles6502 = 6 in
  defm EOR           : Op<0x41, "eor", IndexedIndirect>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in {
    defm EOR           : OpPgX<0x5D, "eor", AbsoluteX>;
    defm EOR           : OpPgX<0x59, "eor", AbsoluteY>;
  }
  let Cycles6502 = 5 in
  defm EOR           : OpPgX<0x51, "eor", IndirectIndexed>;
}

//===----------------------------------------------------------------------===//
// Arithmetic Instructions (ADC, SBC) - Affect N, V, Z, C
//===----------------------------------------------------------------------===//
// ADC/SBC use helper functions that handle decimal mode:
//   doADC(value) - adds value + C to A, sets N, V, Z, C
//   doSBC(value) - subtracts value + !C from A, sets N, V, Z, C

// ADC - Add with Carry
let FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1, Emulate = [{ doADC($Value); }] in {
  let Cycles6502 = 2 in
  def ADC_Immediate  : MOSInst<0x69, "adc", Immediate> { let MHigh = 1; }
  let Cycles6502 = 3 in
  defm ADC           : Op<0x65, "adc", ZeroPage>;
  let Cycles6502 = 4 in
  defm ADC           : Op<0x75, "adc", ZeroPageX>;
  let Cycles6502 = 4 in
  defm ADC           : Op<0x6D, "adc", Absolute>;
  let Cycles6502 = 6 in
  defm ADC           : Op<0x61, "adc", IndexedIndirect>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in {
    defm ADC           : OpPgX<0x7D, "adc", AbsoluteX>;
    defm ADC           : OpPgX<0x79, "adc", AbsoluteY>;
  }
  let Cycles6502 = 5 in
  defm ADC           : OpPgX<0x71, "adc", IndirectIndexed>;
}

// SBC - Subtract with Carry
let FlagN = 1, FlagV = 1, FlagZ = 1, FlagC = 1, Emulate = [{ doSBC($Value); }] in {
  let Cycles6502 = 2 in
  def SBC_Immediate  : MOSInst<0xE9, "sbc", Immediate> { let MHigh = 1; }
  let Cycles6502 = 3 in
  defm SBC           : Op<0xE5, "sbc", ZeroPage>;
  let Cycles6502 = 4 in
  defm SBC           : Op<0xF5, "sbc", ZeroPageX>;
  let Cycles6502 = 4 in
  defm SBC           : Op<0xED, "sbc", Absolute>;
  let Cycles6502 = 6 in
  defm SBC           : Op<0xE1, "sbc", IndexedIndirect>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in {
    defm SBC           : OpPgX<0xFD, "sbc", AbsoluteX>;
    defm SBC           : OpPgX<0xF9, "sbc", AbsoluteY>;
  }
  let Cycles6502 = 5 in
  defm SBC           : OpPgX<0xF1, "sbc", IndirectIndexed>;
}

//===----------------------------------------------------------------------===//
// Compare Instructions (CMP, CPX, CPY) - Affect N, Z, C
//===----------------------------------------------------------------------===//
// Compare sets flags as if subtracting: C = (reg >= value), Z = (reg == value)
// N = bit 7 of (reg - value)

// CMP - Compare Accumulator
let FlagN = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{ uint8_t diff = A - $Value; C = (A >= $Value); setNZ(diff); }] in {
  let Cycles6502 = 2 in
  def CMP_Immediate  : MOSInst<0xC9, "cmp", Immediate> { let MHigh = 1; }
  let Cycles6502 = 3 in
  defm CMP           : Op<0xC5, "cmp", ZeroPage>;
  let Cycles6502 = 4 in
  defm CMP           : Op<0xD5, "cmp", ZeroPageX>;
  let Cycles6502 = 4 in
  defm CMP           : Op<0xCD, "cmp", Absolute>;
  let Cycles6502 = 6 in
  defm CMP           : Op<0xC1, "cmp", IndexedIndirect>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in {
    defm CMP           : OpPgX<0xDD, "cmp", AbsoluteX>;
    defm CMP           : OpPgX<0xD9, "cmp", AbsoluteY>;
  }
  let Cycles6502 = 5 in
  defm CMP           : OpPgX<0xD1, "cmp", IndirectIndexed>;
}

// CPX - Compare X Register
let FlagN = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{ uint8_t diff = X - $Value; C = (X >= $Value); setNZ(diff); }] in {
  let Cycles6502 = 2 in
  def CPX_Immediate  : MOSInst<0xE0, "cpx", Immediate> { let XHigh = 1; }
  let Cycles6502 = 3 in
  defm CPX           : Op<0xE4, "cpx", ZeroPage>;
  let Cycles6502 = 4 in
  defm CPX           : Op<0xEC, "cpx", Absolute>;
}

// CPY - Compare Y Register
let FlagN = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{ uint8_t diff = Y - $Value; C = (Y >= $Value); setNZ(diff); }] in {
  let Cycles6502 = 2 in
  def CPY_Immediate  : MOSInst<0xC0, "cpy", Immediate> { let XHigh = 1; }
  let Cycles6502 = 3 in
  defm CPY           : Op<0xC4, "cpy", ZeroPage>;
  let Cycles6502 = 4 in
  defm CPY           : Op<0xCC, "cpy", Absolute>;
}

//===----------------------------------------------------------------------===//
// BIT Instruction - Affects N, V, Z
//===----------------------------------------------------------------------===//
// BIT sets Z based on A AND Value, but N and V come directly from Value bits 7,6

let FlagN = 1, FlagV = 1, FlagZ = 1,
    Emulate = [{ Z = (A & $Value) == 0; N = ($Value >> 7) & 1; V = ($Value >> 6) & 1; }] in {
  let Cycles6502 = 3 in
  defm BIT           : Op<0x24, "bit", ZeroPage>;
  let Cycles6502 = 4 in
  defm BIT           : Op<0x2C, "bit", Absolute>;
}

//===----------------------------------------------------------------------===//
// Load Instructions (LDA, LDX, LDY) - Affect N, Z
//===----------------------------------------------------------------------===//
// LDA loads a value into the accumulator
// LDX loads a value into the X register
// LDY loads a value into the Y register
// All set N and Z flags based on the loaded value.

// LDA - Load Accumulator (all variants share emulation behavior)
let FlagN = 1, FlagZ = 1, Emulate = [{ A = $Value; setNZ(A); }] in {
  let Cycles6502 = 2 in
  def LDA_Immediate  : MOSInst<0xA9, "lda", Immediate> { let MHigh = 1; }
  let Cycles6502 = 3 in
  defm LDA           : Op<0xA5, "lda", ZeroPage>;
  let Cycles6502 = 4 in
  defm LDA           : Op<0xB5, "lda", ZeroPageX>;
  let Cycles6502 = 4 in
  defm LDA           : Op<0xAD, "lda", Absolute>;
  let Cycles6502 = 6 in
  defm LDA           : Op<0xA1, "lda", IndexedIndirect>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in {
    defm LDA           : OpPgX<0xBD, "lda", AbsoluteX>;
    defm LDA           : OpPgX<0xB9, "lda", AbsoluteY>;
  }
  let Cycles6502 = 5 in
  defm LDA           : OpPgX<0xB1, "lda", IndirectIndexed>;
}

// LDX - Load X Register (all variants share emulation behavior)
let FlagN = 1, FlagZ = 1, Emulate = [{ X = $Value; setNZ(X); }] in {
  let Cycles6502 = 2 in
  def LDX_Immediate  : MOSInst<0xA2, "ldx", Immediate> { let XHigh = 1; }
  let Cycles6502 = 3 in
  defm LDX           : Op<0xA6, "ldx", ZeroPage>;
  let Cycles6502 = 4 in
  defm LDX           : Op<0xB6, "ldx", ZeroPageY>;
  let Cycles6502 = 4 in
  defm LDX           : Op<0xAE, "ldx", Absolute>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in
  defm LDX           : OpPgX<0xBE, "ldx", AbsoluteY>;
}

// LDY - Load Y Register (all variants share emulation behavior)
let FlagN = 1, FlagZ = 1, Emulate = [{ Y = $Value; setNZ(Y); }] in {
  let Cycles6502 = 2 in
  def LDY_Immediate  : MOSInst<0xA0, "ldy", Immediate> { let XHigh = 1; }
  let Cycles6502 = 3 in
  defm LDY           : Op<0xA4, "ldy", ZeroPage>;
  let Cycles6502 = 4 in
  defm LDY           : Op<0xB4, "ldy", ZeroPageX>;
  let Cycles6502 = 4 in
  defm LDY           : Op<0xAC, "ldy", Absolute>;
  // Indexed modes with page-cross penalty
  let Cycles6502 = 4 in
  defm LDY           : OpPgX<0xBC, "ldy", AbsoluteX>;
}

//===----------------------------------------------------------------------===//
// Store Instructions (STA, STX, STY) - No flags affected
//===----------------------------------------------------------------------===//
// STA stores the accumulator to memory
// STX stores the X register to memory
// STY stores the Y register to memory
// Store instructions do not affect any flags.

// STA - Store Accumulator (all variants share emulation behavior)
let Emulate = [{ write($EA, A); }] in {
  // Zero Page: 3 cycles
  let Cycles6502 = 3 in
  defm STA           : Op<0x85, "sta", ZeroPage>;

  // Zero Page,X: 4 cycles
  let Cycles6502 = 4 in
  defm STA           : Op<0x95, "sta", ZeroPageX>;

  // Absolute: 4 cycles
  let Cycles6502 = 4 in
  defm STA           : Op<0x8D, "sta", Absolute>;

  // Absolute,X: 5 cycles (no page cross penalty for stores)
  let Cycles6502 = 5 in
  defm STA           : Op<0x9D, "sta", AbsoluteX>;

  // Absolute,Y: 5 cycles (no page cross penalty for stores)
  let Cycles6502 = 5 in
  defm STA           : Op<0x99, "sta", AbsoluteY>;

  // Indexed Indirect (d,X): 6 cycles
  let Cycles6502 = 6 in
  defm STA           : Op<0x81, "sta", IndexedIndirect>;

  // Indirect Indexed (d),Y: 6 cycles (no page cross penalty for stores)
  let Cycles6502 = 6 in
  defm STA           : Op<0x91, "sta", IndirectIndexed>;
}

// STX - Store X Register (all variants share emulation behavior)
let Emulate = [{ write($EA, X); }] in {
  // Zero Page: 3 cycles
  let Cycles6502 = 3 in
  defm STX           : Op<0x86, "stx", ZeroPage>;

  // Zero Page,Y: 4 cycles
  let Cycles6502 = 4 in
  defm STX           : Op<0x96, "stx", ZeroPageY>;

  // Absolute: 4 cycles
  let Cycles6502 = 4 in
  defm STX           : Op<0x8E, "stx", Absolute>;
}

// STY - Store Y Register (all variants share emulation behavior)
let Emulate = [{ write($EA, Y); }] in {
  // Zero Page: 3 cycles
  let Cycles6502 = 3 in
  defm STY           : Op<0x84, "sty", ZeroPage>;

  // Zero Page,X: 4 cycles
  let Cycles6502 = 4 in
  defm STY           : Op<0x94, "sty", ZeroPageX>;

  // Absolute: 4 cycles
  let Cycles6502 = 4 in
  defm STY           : Op<0x8C, "sty", Absolute>;
}

//===----------------------------------------------------------------------===//
// Shift/Rotate Instructions (ASL, LSR, ROL, ROR) - Affect N, Z, C
//===----------------------------------------------------------------------===//

// ASL - Arithmetic Shift Left (shifts 0 into bit 0, bit 7 into C)
// Accumulator mode
let FlagN = 1, FlagZ = 1, FlagC = 1, Cycles6502 = 2,
    Emulate = [{ C = (A >> 7) & 1; A <<= 1; setNZ(A); }] in
defm ASL           : Op<0x0A, "asl", Accumulator>;
// Memory modes (read-modify-write)
let FlagN = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{ uint8_t v = $Value; C = (v >> 7) & 1; v <<= 1; write($EA, v); setNZ(v); }] in {
  let Cycles6502 = 5 in
  defm ASL           : Op<0x06, "asl", ZeroPage>;
  let Cycles6502 = 6 in {
  defm ASL           : Op<0x16, "asl", ZeroPageX>;
  defm ASL           : Op<0x0E, "asl", Absolute>;
  }
  let Cycles6502 = 7 in
  defm ASL           : Op<0x1E, "asl", AbsoluteX>;
}

// LSR - Logical Shift Right (shifts 0 into bit 7, bit 0 into C)
// Accumulator mode
let FlagN = 1, FlagZ = 1, FlagC = 1, Cycles6502 = 2,
    Emulate = [{ C = A & 1; A >>= 1; setNZ(A); }] in
defm LSR           : Op<0x4A, "lsr", Accumulator>;
// Memory modes
let FlagN = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{ uint8_t v = $Value; C = v & 1; v >>= 1; write($EA, v); setNZ(v); }] in {
  let Cycles6502 = 5 in
  defm LSR           : Op<0x46, "lsr", ZeroPage>;
  let Cycles6502 = 6 in {
  defm LSR           : Op<0x56, "lsr", ZeroPageX>;
  defm LSR           : Op<0x4E, "lsr", Absolute>;
  }
  let Cycles6502 = 7 in
  defm LSR           : Op<0x5E, "lsr", AbsoluteX>;
}

// ROL - Rotate Left (C into bit 0, bit 7 into C)
// Accumulator mode
let FlagN = 1, FlagZ = 1, FlagC = 1, Cycles6502 = 2,
    Emulate = [{ bool c = C; C = (A >> 7) & 1; A = (A << 1) | c; setNZ(A); }] in
defm ROL           : Op<0x2A, "rol", Accumulator>;
// Memory modes
let FlagN = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{ uint8_t v = $Value; bool c = C; C = (v >> 7) & 1; v = (v << 1) | c; write($EA, v); setNZ(v); }] in {
  let Cycles6502 = 5 in
  defm ROL           : Op<0x26, "rol", ZeroPage>;
  let Cycles6502 = 6 in {
  defm ROL           : Op<0x36, "rol", ZeroPageX>;
  defm ROL           : Op<0x2E, "rol", Absolute>;
  }
  let Cycles6502 = 7 in
  defm ROL           : Op<0x3E, "rol", AbsoluteX>;
}

// ROR - Rotate Right (C into bit 7, bit 0 into C)
// Accumulator mode
let FlagN = 1, FlagZ = 1, FlagC = 1, Cycles6502 = 2,
    Emulate = [{ bool c = C; C = A & 1; A = (A >> 1) | (c << 7); setNZ(A); }] in
defm ROR           : Op<0x6A, "ror", Accumulator>;
// Memory modes
let FlagN = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{ uint8_t v = $Value; bool c = C; C = v & 1; v = (v >> 1) | (c << 7); write($EA, v); setNZ(v); }] in {
  let Cycles6502 = 5 in
  defm ROR           : Op<0x66, "ror", ZeroPage>;
  let Cycles6502 = 6 in {
  defm ROR           : Op<0x76, "ror", ZeroPageX>;
  defm ROR           : Op<0x6E, "ror", Absolute>;
  }
  let Cycles6502 = 7 in
  defm ROR           : Op<0x7E, "ror", AbsoluteX>;
}

//===----------------------------------------------------------------------===//
// Increment/Decrement Instructions (INC, DEC, INX, DEX, INY, DEY) - Affect N, Z
//===----------------------------------------------------------------------===//

// DEC - Decrement Memory
let FlagN = 1, FlagZ = 1, Emulate = [{ uint8_t v = $Value - 1; write($EA, v); setNZ(v); }] in {
  let Cycles6502 = 5 in
  defm DEC           : Op<0xC6, "dec", ZeroPage>;
  let Cycles6502 = 6 in {
  defm DEC           : Op<0xD6, "dec", ZeroPageX>;
  defm DEC           : Op<0xCE, "dec", Absolute>;
  }
  let Cycles6502 = 7 in
  defm DEC           : Op<0xDE, "dec", AbsoluteX>;
}

// INC - Increment Memory
let FlagN = 1, FlagZ = 1, Emulate = [{ uint8_t v = $Value + 1; write($EA, v); setNZ(v); }] in {
  let Cycles6502 = 5 in
  defm INC           : Op<0xE6, "inc", ZeroPage>;
  let Cycles6502 = 6 in {
  defm INC           : Op<0xF6, "inc", ZeroPageX>;
  defm INC           : Op<0xEE, "inc", Absolute>;
  }
  let Cycles6502 = 7 in
  defm INC           : Op<0xFE, "inc", AbsoluteX>;
}

// Register operations: 2 cycles
let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
let Emulate = [{ Y--; setNZ(Y); }] in
defm DEY           : Op<0x88, "dey", Implicit>;

let Emulate = [{ Y++; setNZ(Y); }] in
defm INY           : Op<0xC8, "iny", Implicit>;

let Emulate = [{ X--; setNZ(X); }] in
defm DEX           : Op<0xCA, "dex", Implicit>;

let Emulate = [{ X++; setNZ(X); }] in
defm INX           : Op<0xE8, "inx", Implicit>;
}

//===----------------------------------------------------------------------===//
// Transfer Instructions - Affect N, Z (except TXS)
//===----------------------------------------------------------------------===//

let Cycles6502 = 2, FlagN = 1, FlagZ = 1 in {
let Emulate = [{ X = A; setNZ(X); }] in
defm TAX           : Op<0xAA, "tax", Implicit>;

let Emulate = [{ Y = A; setNZ(Y); }] in
defm TAY           : Op<0xA8, "tay", Implicit>;

let Emulate = [{ A = X; setNZ(A); }] in
defm TXA           : Op<0x8A, "txa", Implicit>;

let Emulate = [{ A = Y; setNZ(A); }] in
defm TYA           : Op<0x98, "tya", Implicit>;

let Emulate = [{ X = S; setNZ(X); }] in
defm TSX           : Op<0xBA, "tsx", Implicit>;
}

// TXS doesn't affect any flags
let Cycles6502 = 2, Emulate = [{ S = X; }] in {
defm TXS           : Op<0x9A, "txs", Implicit>;
}

//===----------------------------------------------------------------------===//
// Flag Instructions - Affect specific flags
//===----------------------------------------------------------------------===//

let Cycles6502 = 2 in {
// Clear flags (set to 0)
let FlagC = 3, Emulate = [{ C = false; }] in
defm CLC : Op<0x18, "clc", Implicit>;  // Clear Carry

let FlagI = 3, Emulate = [{ I = false; }] in
defm CLI : Op<0x58, "cli", Implicit>;  // Clear Interrupt Disable

let FlagV = 3, Emulate = [{ V = false; }] in
defm CLV : Op<0xB8, "clv", Implicit>;  // Clear Overflow

let FlagD = 3, Emulate = [{ D = false; }] in
defm CLD : Op<0xD8, "cld", Implicit>;  // Clear Decimal

// Set flags (set to 1)
let FlagC = 2, Emulate = [{ C = true; }] in
defm SEC : Op<0x38, "sec", Implicit>;  // Set Carry

let FlagI = 2, Emulate = [{ I = true; }] in
defm SEI : Op<0x78, "sei", Implicit>;  // Set Interrupt Disable

let FlagD = 2, Emulate = [{ D = true; }] in
defm SED : Op<0xF8, "sed", Implicit>;  // Set Decimal
}

//===----------------------------------------------------------------------===//
// Stack Operations
//===----------------------------------------------------------------------===//

// Push: 3 cycles, no flag effects
let Cycles6502 = 3 in {
let Emulate = [{ push(getP() | 0x30); }] in  // B flag set when pushed
defm PHP           : Op<0x08, "php", Implicit>;

let Emulate = [{ push(A); }] in
defm PHA           : Op<0x48, "pha", Implicit>;
}

// PLA: 4 cycles, affects N, Z
let Cycles6502 = 4, FlagN = 1, FlagZ = 1, Emulate = [{ A = pull(); setNZ(A); }] in {
defm PLA           : Op<0x68, "pla", Implicit>;
}

// PLP: 4 cycles, restores all flags
let Cycles6502 = 4, FlagN = 1, FlagV = 1, FlagB = 1, FlagD = 1, FlagI = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{ setP(pull()); }] in {
defm PLP           : Op<0x28, "plp", Implicit>;
}

//===----------------------------------------------------------------------===//
// No-Operation
//===----------------------------------------------------------------------===//

let Cycles6502 = 2, Emulate = [{ /* NOP does nothing */ }] in {
defm NOP           : Op<0xEA, "nop", Implicit>;
}

//===----------------------------------------------------------------------===//
// Branch Instructions - No flag effects
//===----------------------------------------------------------------------===//
// Branches take 2 cycles if not taken, +1 if taken, +1 more if page cross

let Cycles6502 = 2, PageCrossCycles = 1 in {
let Emulate = [{ if (!N) { PC = $EA; PCModified = true; Cycles++; } }] in
defm BPL           : OpBranch<0x10, "bpl">;  // Branch if Plus (N=0)
let Emulate = [{ if (N) { PC = $EA; PCModified = true; Cycles++; } }] in
defm BMI           : OpBranch<0x30, "bmi">;  // Branch if Minus (N=1)
let Emulate = [{ if (!V) { PC = $EA; PCModified = true; Cycles++; } }] in
defm BVC           : OpBranch<0x50, "bvc">;  // Branch if Overflow Clear (V=0)
let Emulate = [{ if (V) { PC = $EA; PCModified = true; Cycles++; } }] in
defm BVS           : OpBranch<0x70, "bvs">;  // Branch if Overflow Set (V=1)
let Emulate = [{ if (!C) { PC = $EA; PCModified = true; Cycles++; } }] in
defm BCC           : OpBranch<0x90, "bcc">;  // Branch if Carry Clear (C=0)
let Emulate = [{ if (C) { PC = $EA; PCModified = true; Cycles++; } }] in
defm BCS           : OpBranch<0xB0, "bcs">;  // Branch if Carry Set (C=1)
let Emulate = [{ if (!Z) { PC = $EA; PCModified = true; Cycles++; } }] in
defm BNE           : OpBranch<0xD0, "bne">;  // Branch if Not Equal (Z=0)
let Emulate = [{ if (Z) { PC = $EA; PCModified = true; Cycles++; } }] in
defm BEQ           : OpBranch<0xF0, "beq">;  // Branch if Equal (Z=1)
}

//===----------------------------------------------------------------------===//
// Jump/Call/Return Instructions
//===----------------------------------------------------------------------===//

// JMP - Jump (unconditional)
let Cycles6502 = 3, Emulate = [{ PC = $EA; PCModified = true; }] in
defm JMP           : OpJump<0x4C, "jmp", Absolute>;

let Cycles6502 = 5, Emulate = [{ PC = $EA; PCModified = true; }] in
defm JMP           : OpJumpIndirect<0x6C, "jmp", Indirect16>;

// JSR - Jump to Subroutine (pushes return address - 1)
let Cycles6502 = 6, Emulate = [{
  uint16_t ret = PC + 2;  // PC of last byte of JSR instruction
  push((ret >> 8) & 0xFF);
  push(ret & 0xFF);
  PC = $EA;
  PCModified = true;
}] in
defm JSR           : OpCall<0x20, "jsr", Absolute>;

// RTS - Return from Subroutine (pulls return address, adds 1)
let Cycles6502 = 6, Emulate = [{
  uint8_t lo = pull();
  uint8_t hi = pull();
  PC = ((hi << 8) | lo) + 1;
  PCModified = true;
}] in
defm RTS           : OpReturn<0x60, "rts">;

// RTI - Return from Interrupt (pulls P then PC)
let Cycles6502 = 6, FlagN = 1, FlagV = 1, FlagB = 1, FlagD = 1, FlagI = 1, FlagZ = 1, FlagC = 1,
    Emulate = [{
  setP(pull());
  uint8_t lo = pull();
  uint8_t hi = pull();
  PC = (hi << 8) | lo;
  PCModified = true;
}] in
defm RTI           : OpReturn<0x40, "rti">;

//===----------------------------------------------------------------------===//
// Aliases
//===----------------------------------------------------------------------===//

def : InstAlias<"asl a", (ASL_Accumulator)>;
def : InstAlias<"rol a", (ROL_Accumulator)>;
def : InstAlias<"lsr a", (LSR_Accumulator)>;
def : InstAlias<"ror a", (ROR_Accumulator)>;

} // Predicates = [Has6502]
