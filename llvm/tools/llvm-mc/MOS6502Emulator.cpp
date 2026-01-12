//===-- MOS6502Emulator.cpp - 6502 Emulator for llvm-mc ---------*- C++ -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements a simple 6502 emulator for use with llvm-mc --run.
//
// NOTE: This is a prototype. Eventually this will be auto-generated from
// MOSEmulator.td definitions by a TableGen backend.
//
//===----------------------------------------------------------------------===//

#include "MOS6502Emulator.h"

// Include the generated instruction enum
#ifdef MOS_TARGET_AVAILABLE
#include "MOS/MOSGenInstrInfo.inc"
#define MOS_OPCODE(x) MOS::x
#else
// Fallback definitions for standalone testing
// These values come from the generated MOSGenInstrInfo.inc
namespace MOS {
enum {
  // Load A
  LDA_Immediate = 719,
  LDA_ZeroPage = 732,
  LDA_ZeroPageX = 733,
  LDA_Absolute = 713,
  LDA_AbsoluteX = 715,
  LDA_AbsoluteY = 717,
  LDA_IndexedIndirect = 721,
  LDA_IndirectIndexed = 723,

  // Load X
  LDX_Immediate = 740,
  LDX_ZeroPage = 742,
  LDX_ZeroPageY = 743,
  LDX_Absolute = 738,
  LDX_AbsoluteY = 739,

  // Load Y
  LDY_Immediate = 746,
  LDY_ZeroPage = 748,
  LDY_ZeroPageX = 749,
  LDY_Absolute = 744,
  LDY_AbsoluteX = 745,

  // Store A
  STA_ZeroPage = 1170,
  STA_ZeroPageX = 1171,
  STA_Absolute = 1153,
  STA_AbsoluteX = 1155,
  STA_AbsoluteY = 1157,
  STA_IndexedIndirect = 1159,
  STA_IndirectIndexed = 1161,

  // Store X
  STX_ZeroPage = 1179,
  STX_ZeroPageY = 1180,
  STX_Absolute = 1177,

  // Store Y
  STY_ZeroPage = 1183,
  STY_ZeroPageX = 1184,
  STY_Absolute = 1181,

  // Transfers
  TAX_Implied = 1195,
  TAY_Implied = 1196,
  TXA_Implied = 1221,
  TYA_Implied = 1226,
  TSX_Implied = 1219,
  TXS_Implied = 1224,

  // Logic
  AND_Immediate = 513,
  ORA_Immediate = 785,
  EOR_Immediate = 660,

  // Arithmetic
  ADC_Immediate = 487,
  SBC_Immediate = 895,

  // Compare
  CMP_Immediate = 597,
  CPX_Immediate = 612,
  CPY_Immediate = 616,

  // Increment/Decrement registers
  INX_Implied = 685,
  INY_Implied = 686,
  DEX_Implied = 642,
  DEY_Implied = 643,

  // Increment/Decrement memory
  INC_ZeroPage = 677,
  DEC_ZeroPage = 634,

  // Shifts
  ASL_Accumulator = 536,
  LSR_Accumulator = 760,
  ROL_Accumulator = 853,
  ROR_Accumulator = 863,

  // Branches
  BEQ_Relative = 552,
  BNE_Relative = 564,
  BCS_Relative = 550,
  BCC_Relative = 548,
  BMI_Relative = 562,
  BPL_Relative = 566,
  BVS_Relative = 577,
  BVC_Relative = 575,

  // Stack
  PHA_Implied = 807,
  PHP_Implied = 812,
  PLA_Implied = 818,
  PLP_Implied = 822,

  // Jumps/Calls/Returns
  JMP_Absolute = 696,
  JMP_Indirect16 = 699,
  JSR_Absolute = 701,
  RTS_Implied = 877,
  RTI_Implied = 874,
  BRK_Implied = 571,

  // Flags
  CLC_Implied = 580,
  SEC_Implied = 911,
  CLI_Implied = 583,
  SEI_Implied = 914,
  CLD_Implied = 581,
  SED_Implied = 912,
  CLV_Implied = 584,

  // BIT
  BIT_ZeroPage = 560,
  BIT_Absolute = 556,

  // NOP
  NOP_Implied = 774,
};
} // namespace MOS
#define MOS_OPCODE(x) MOS::x
#endif

using namespace llvm;

unsigned MOS6502Emulator::emulateInstruction(const MCInst &Inst) {
  unsigned Opcode = Inst.getOpcode();

  // Helper macros for addressing modes
  // These correspond to the EmulatorAddrMode definitions in MOSEmulator.td

#define IMM8 ((uint8_t)Inst.getOperand(0).getImm())
#define ZP ((uint16_t)Inst.getOperand(0).getImm())
#define ZPX ((uint8_t)(Inst.getOperand(0).getImm() + X))
#define ZPY ((uint8_t)(Inst.getOperand(0).getImm() + Y))
#define ABS ((uint16_t)Inst.getOperand(0).getImm())
#define ABSX_BASE ((uint16_t)Inst.getOperand(0).getImm())
#define ABSX ((uint16_t)(ABSX_BASE + X))
#define ABSY_BASE ((uint16_t)Inst.getOperand(0).getImm())
#define ABSY ((uint16_t)(ABSY_BASE + Y))
#define INDX_EA ({ \
    uint8_t zp = (uint8_t)(Inst.getOperand(0).getImm() + X); \
    (uint16_t)(read(zp) | (read((uint8_t)(zp + 1)) << 8)); \
  })
#define INDY_BASE ({ \
    uint8_t zp = Inst.getOperand(0).getImm(); \
    (uint16_t)(read(zp) | (read((uint8_t)(zp + 1)) << 8)); \
  })
#define INDY ((uint16_t)(INDY_BASE + Y))
#define REL ((uint16_t)(PC + 2 + (int8_t)Inst.getOperand(0).getImm()))

  switch (Opcode) {
  //===------------------------------------------------------------------===//
  // Load Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(LDA_Immediate):
    A = IMM8;
    setNZ(A);
    Cycles += 2;
    return 2;

  case MOS_OPCODE(LDA_ZeroPage):
    A = read(ZP);
    setNZ(A);
    Cycles += 3;
    return 2;

  case MOS_OPCODE(LDA_ZeroPageX):
    A = read(ZPX);
    setNZ(A);
    Cycles += 4;
    return 2;

  case MOS_OPCODE(LDA_Absolute):
    A = read(ABS);
    setNZ(A);
    Cycles += 4;
    return 3;

  case MOS_OPCODE(LDA_AbsoluteX): {
    uint16_t base = ABSX_BASE;
    uint16_t ea = base + X;
    A = read(ea);
    setNZ(A);
    Cycles += 4;
    if (pageCrossed(base, ea))
      Cycles += 1;
    return 3;
  }

  case MOS_OPCODE(LDA_AbsoluteY): {
    uint16_t base = ABSY_BASE;
    uint16_t ea = base + Y;
    A = read(ea);
    setNZ(A);
    Cycles += 4;
    if (pageCrossed(base, ea))
      Cycles += 1;
    return 3;
  }

  case MOS_OPCODE(LDA_IndexedIndirect):
    A = read(INDX_EA);
    setNZ(A);
    Cycles += 6;
    return 2;

  case MOS_OPCODE(LDA_IndirectIndexed): {
    uint16_t base = INDY_BASE;
    uint16_t ea = base + Y;
    A = read(ea);
    setNZ(A);
    Cycles += 5;
    if (pageCrossed(base, ea))
      Cycles += 1;
    return 2;
  }

  case MOS_OPCODE(LDX_Immediate):
    X = IMM8;
    setNZ(X);
    Cycles += 2;
    return 2;

  case MOS_OPCODE(LDX_ZeroPage):
    X = read(ZP);
    setNZ(X);
    Cycles += 3;
    return 2;

  case MOS_OPCODE(LDX_ZeroPageY):
    X = read(ZPY);
    setNZ(X);
    Cycles += 4;
    return 2;

  case MOS_OPCODE(LDX_Absolute):
    X = read(ABS);
    setNZ(X);
    Cycles += 4;
    return 3;

  case MOS_OPCODE(LDX_AbsoluteY): {
    uint16_t base = ABSY_BASE;
    uint16_t ea = base + Y;
    X = read(ea);
    setNZ(X);
    Cycles += 4;
    if (pageCrossed(base, ea))
      Cycles += 1;
    return 3;
  }

  case MOS_OPCODE(LDY_Immediate):
    Y = IMM8;
    setNZ(Y);
    Cycles += 2;
    return 2;

  case MOS_OPCODE(LDY_ZeroPage):
    Y = read(ZP);
    setNZ(Y);
    Cycles += 3;
    return 2;

  case MOS_OPCODE(LDY_ZeroPageX):
    Y = read(ZPX);
    setNZ(Y);
    Cycles += 4;
    return 2;

  case MOS_OPCODE(LDY_Absolute):
    Y = read(ABS);
    setNZ(Y);
    Cycles += 4;
    return 3;

  case MOS_OPCODE(LDY_AbsoluteX): {
    uint16_t base = ABSX_BASE;
    uint16_t ea = base + X;
    Y = read(ea);
    setNZ(Y);
    Cycles += 4;
    if (pageCrossed(base, ea))
      Cycles += 1;
    return 3;
  }

  //===------------------------------------------------------------------===//
  // Store Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(STA_ZeroPage):
    write(ZP, A);
    Cycles += 3;
    return 2;

  case MOS_OPCODE(STA_ZeroPageX):
    write(ZPX, A);
    Cycles += 4;
    return 2;

  case MOS_OPCODE(STA_Absolute):
    write(ABS, A);
    Cycles += 4;
    return 3;

  case MOS_OPCODE(STA_AbsoluteX):
    write(ABSX, A);
    Cycles += 5;
    return 3;

  case MOS_OPCODE(STA_AbsoluteY):
    write(ABSY, A);
    Cycles += 5;
    return 3;

  case MOS_OPCODE(STA_IndexedIndirect):
    write(INDX_EA, A);
    Cycles += 6;
    return 2;

  case MOS_OPCODE(STA_IndirectIndexed):
    write(INDY, A);
    Cycles += 6;
    return 2;

  case MOS_OPCODE(STX_ZeroPage):
    write(ZP, X);
    Cycles += 3;
    return 2;

  case MOS_OPCODE(STX_ZeroPageY):
    write(ZPY, X);
    Cycles += 4;
    return 2;

  case MOS_OPCODE(STX_Absolute):
    write(ABS, X);
    Cycles += 4;
    return 3;

  case MOS_OPCODE(STY_ZeroPage):
    write(ZP, Y);
    Cycles += 3;
    return 2;

  case MOS_OPCODE(STY_ZeroPageX):
    write(ZPX, Y);
    Cycles += 4;
    return 2;

  case MOS_OPCODE(STY_Absolute):
    write(ABS, Y);
    Cycles += 4;
    return 3;

  //===------------------------------------------------------------------===//
  // Transfer Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(TAX_Implied):
    X = A;
    setNZ(X);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(TAY_Implied):
    Y = A;
    setNZ(Y);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(TXA_Implied):
    A = X;
    setNZ(A);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(TYA_Implied):
    A = Y;
    setNZ(A);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(TSX_Implied):
    X = S;
    setNZ(X);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(TXS_Implied):
    S = X;
    Cycles += 2;
    return 1;

  //===------------------------------------------------------------------===//
  // Logic Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(AND_Immediate):
    A = A & IMM8;
    setNZ(A);
    Cycles += 2;
    return 2;

  case MOS_OPCODE(ORA_Immediate):
    A = A | IMM8;
    setNZ(A);
    Cycles += 2;
    return 2;

  case MOS_OPCODE(EOR_Immediate):
    A = A ^ IMM8;
    setNZ(A);
    Cycles += 2;
    return 2;

  //===------------------------------------------------------------------===//
  // Arithmetic Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(ADC_Immediate): {
    uint8_t val = IMM8;
    uint16_t sum = A + val + C;
    C = sum > 0xFF;
    V = ~(A ^ val) & (A ^ sum) & 0x80;
    A = sum & 0xFF;
    setNZ(A);
    Cycles += 2;
    return 2;
  }

  case MOS_OPCODE(SBC_Immediate): {
    uint8_t val = IMM8;
    uint16_t diff = A - val - !C;
    C = diff <= 0xFF; // No borrow
    V = (A ^ val) & (A ^ diff) & 0x80;
    A = diff & 0xFF;
    setNZ(A);
    Cycles += 2;
    return 2;
  }

  //===------------------------------------------------------------------===//
  // Compare Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(CMP_Immediate): {
    uint8_t val = IMM8;
    uint8_t diff = A - val;
    C = A >= val;
    setNZ(diff);
    Cycles += 2;
    return 2;
  }

  case MOS_OPCODE(CPX_Immediate): {
    uint8_t val = IMM8;
    uint8_t diff = X - val;
    C = X >= val;
    setNZ(diff);
    Cycles += 2;
    return 2;
  }

  case MOS_OPCODE(CPY_Immediate): {
    uint8_t val = IMM8;
    uint8_t diff = Y - val;
    C = Y >= val;
    setNZ(diff);
    Cycles += 2;
    return 2;
  }

  //===------------------------------------------------------------------===//
  // Increment/Decrement Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(INX_Implied):
    X++;
    setNZ(X);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(INY_Implied):
    Y++;
    setNZ(Y);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(DEX_Implied):
    X--;
    setNZ(X);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(DEY_Implied):
    Y--;
    setNZ(Y);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(INC_ZeroPage): {
    uint16_t ea = ZP;
    uint8_t val = read(ea);
    val++;
    setNZ(val);
    write(ea, val);
    Cycles += 5;
    return 2;
  }

  case MOS_OPCODE(DEC_ZeroPage): {
    uint16_t ea = ZP;
    uint8_t val = read(ea);
    val--;
    setNZ(val);
    write(ea, val);
    Cycles += 5;
    return 2;
  }

  //===------------------------------------------------------------------===//
  // Shift/Rotate Instructions (Accumulator only for now)
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(ASL_Accumulator):
    C = A >> 7;
    A <<= 1;
    setNZ(A);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(LSR_Accumulator):
    C = A & 1;
    A >>= 1;
    setNZ(A);
    Cycles += 2;
    return 1;

  case MOS_OPCODE(ROL_Accumulator): {
    uint8_t c = C;
    C = A >> 7;
    A = (A << 1) | c;
    setNZ(A);
    Cycles += 2;
    return 1;
  }

  case MOS_OPCODE(ROR_Accumulator): {
    uint8_t c = C;
    C = A & 1;
    A = (A >> 1) | (c << 7);
    setNZ(A);
    Cycles += 2;
    return 1;
  }

  //===------------------------------------------------------------------===//
  // Branch Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(BEQ_Relative): {
    Cycles += 2;
    if (Z) {
      uint16_t target = REL;
      Cycles += 1;
      if (pageCrossed(PC + 2, target))
        Cycles += 1;
      PC = target;
      return 0; // Don't advance PC
    }
    return 2;
  }

  case MOS_OPCODE(BNE_Relative): {
    Cycles += 2;
    if (!Z) {
      uint16_t target = REL;
      Cycles += 1;
      if (pageCrossed(PC + 2, target))
        Cycles += 1;
      PC = target;
      return 0;
    }
    return 2;
  }

  case MOS_OPCODE(BCS_Relative): {
    Cycles += 2;
    if (C) {
      uint16_t target = REL;
      Cycles += 1;
      if (pageCrossed(PC + 2, target))
        Cycles += 1;
      PC = target;
      return 0;
    }
    return 2;
  }

  case MOS_OPCODE(BCC_Relative): {
    Cycles += 2;
    if (!C) {
      uint16_t target = REL;
      Cycles += 1;
      if (pageCrossed(PC + 2, target))
        Cycles += 1;
      PC = target;
      return 0;
    }
    return 2;
  }

  case MOS_OPCODE(BMI_Relative): {
    Cycles += 2;
    if (N) {
      uint16_t target = REL;
      Cycles += 1;
      if (pageCrossed(PC + 2, target))
        Cycles += 1;
      PC = target;
      return 0;
    }
    return 2;
  }

  case MOS_OPCODE(BPL_Relative): {
    Cycles += 2;
    if (!N) {
      uint16_t target = REL;
      Cycles += 1;
      if (pageCrossed(PC + 2, target))
        Cycles += 1;
      PC = target;
      return 0;
    }
    return 2;
  }

  case MOS_OPCODE(BVS_Relative): {
    Cycles += 2;
    if (V) {
      uint16_t target = REL;
      Cycles += 1;
      if (pageCrossed(PC + 2, target))
        Cycles += 1;
      PC = target;
      return 0;
    }
    return 2;
  }

  case MOS_OPCODE(BVC_Relative): {
    Cycles += 2;
    if (!V) {
      uint16_t target = REL;
      Cycles += 1;
      if (pageCrossed(PC + 2, target))
        Cycles += 1;
      PC = target;
      return 0;
    }
    return 2;
  }

  //===------------------------------------------------------------------===//
  // Stack Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(PHA_Implied):
    push(A);
    Cycles += 3;
    return 1;

  case MOS_OPCODE(PHP_Implied):
    push(getP() | 0x30); // B flag set when pushed
    Cycles += 3;
    return 1;

  case MOS_OPCODE(PLA_Implied):
    A = pull();
    setNZ(A);
    Cycles += 4;
    return 1;

  case MOS_OPCODE(PLP_Implied):
    setP(pull());
    Cycles += 4;
    return 1;

  //===------------------------------------------------------------------===//
  // Jump/Call/Return Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(JMP_Absolute):
    PC = ABS;
    Cycles += 3;
    return 0;

  case MOS_OPCODE(JMP_Indirect16): {
    // 6502 page-wrap bug
    uint16_t ptr = ABS;
    uint8_t lo = read(ptr);
    uint8_t hi = read((ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    PC = lo | (hi << 8);
    Cycles += 5;
    return 0;
  }

  case MOS_OPCODE(JSR_Absolute):
    push16(PC + 2); // Push return address - 1
    PC = ABS;
    Cycles += 6;
    return 0;

  case MOS_OPCODE(RTS_Implied):
    PC = pull16() + 1;
    Cycles += 6;
    return 0;

  case MOS_OPCODE(RTI_Implied):
    setP(pull());
    PC = pull16();
    Cycles += 6;
    return 0;

  case MOS_OPCODE(BRK_Implied):
    push16(PC + 2);
    push(getP() | 0x30);
    I = true;
    PC = read(0xFFFE) | (read(0xFFFF) << 8);
    Cycles += 7;
    Halted = true; // For llvm-mc, treat BRK as halt
    return 0;

  //===------------------------------------------------------------------===//
  // Flag Instructions
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(CLC_Implied):
    C = false;
    Cycles += 2;
    return 1;

  case MOS_OPCODE(SEC_Implied):
    C = true;
    Cycles += 2;
    return 1;

  case MOS_OPCODE(CLI_Implied):
    I = false;
    Cycles += 2;
    return 1;

  case MOS_OPCODE(SEI_Implied):
    I = true;
    Cycles += 2;
    return 1;

  case MOS_OPCODE(CLD_Implied):
    D = false;
    Cycles += 2;
    return 1;

  case MOS_OPCODE(SED_Implied):
    D = true;
    Cycles += 2;
    return 1;

  case MOS_OPCODE(CLV_Implied):
    V = false;
    Cycles += 2;
    return 1;

  //===------------------------------------------------------------------===//
  // BIT Instruction
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(BIT_ZeroPage): {
    uint8_t val = read(ZP);
    Z = (A & val) == 0;
    N = val >> 7;
    V = (val >> 6) & 1;
    Cycles += 3;
    return 2;
  }

  case MOS_OPCODE(BIT_Absolute): {
    uint8_t val = read(ABS);
    Z = (A & val) == 0;
    N = val >> 7;
    V = (val >> 6) & 1;
    Cycles += 4;
    return 3;
  }

  //===------------------------------------------------------------------===//
  // NOP
  //===------------------------------------------------------------------===//

  case MOS_OPCODE(NOP_Implied):
    Cycles += 2;
    return 1;

  default:
    // Unknown instruction - halt
    Halted = true;
    return 0;
  }

#undef IMM8
#undef ZP
#undef ZPX
#undef ZPY
#undef ABS
#undef ABSX_BASE
#undef ABSX
#undef ABSY_BASE
#undef ABSY
#undef INDX_EA
#undef INDY_BASE
#undef INDY
#undef REL
}

unsigned MOS6502Emulator::getInstSize(unsigned Opcode) const {
  // This should be derived from TSFlags or the instruction definition.
  // For now, return based on addressing mode patterns.
  // In the generated version, this will come from the Inst reference.
  return 1; // Placeholder - the execute loop will use the returned value
}
