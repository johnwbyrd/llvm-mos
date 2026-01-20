// MOS 65C02 SAIL Specification
// CMOS 65C02 - extends base NMOS 6502
//
// Inheritance: 6502 (base) -> 65C02
//
// Include this file AFTER the base model files to enable 65C02 features.
// This file contains EVERYTHING specific to the 65C02 variant:
//   - Hardware capability enable
//   - Cycle timing overrides
//   - New instructions
//   - New addressing modes
//
// To create a variant that extends 65C02 (e.g., R65C02), create a new file
// that includes this one first, then adds its own extensions.

// =============================================================================
// Hardware Capability
// =============================================================================

function clause hardwareCapability(Ext_65C02) = true

// =============================================================================
// Cycle Timing Overrides
// =============================================================================

// The 65C02 optimized RMW instructions on indexed addressing modes.
// On NMOS 6502, RMW Absolute,X always takes 7 cycles (dummy read happens).
// On CMOS 65C02, it takes 6 cycles + 1 if page boundary crossed.

function clause baseCycles(AM_AbsoluteX, OC_RMW) = (6, 1)  // was (7, 0) on NMOS
function clause baseCycles(AM_AbsoluteY, OC_RMW) = (6, 1)  // was (7, 0) on NMOS

// =============================================================================
// New Instructions
// =============================================================================

// -----------------------------------------------------------------------------
// Stack Operations: PHX, PHY, PLX, PLY
// -----------------------------------------------------------------------------

union clause instruction = PHX_Implied : unit
function clause execute PHX_Implied() = {
  addCycles(CYCLES_PUSH);
  push(X);
  RETIRE_SUCCESS
}

union clause instruction = PHY_Implied : unit
function clause execute PHY_Implied() = {
  addCycles(CYCLES_PUSH);
  push(Y);
  RETIRE_SUCCESS
}

union clause instruction = PLX_Implied : unit
function clause execute PLX_Implied() = {
  addCycles(CYCLES_PULL);
  X = pull();
  setNZ(X);
  RETIRE_SUCCESS
}

union clause instruction = PLY_Implied : unit
function clause execute PLY_Implied() = {
  addCycles(CYCLES_PULL);
  Y = pull();
  setNZ(Y);
  RETIRE_SUCCESS
}

// -----------------------------------------------------------------------------
// BRA - Branch Always (unconditional relative branch)
// -----------------------------------------------------------------------------

union clause instruction = BRA_Relative : bits(8)
function clause execute BRA_Relative(Offset) = {
  // Always taken, so 3 cycles minimum, +1 if page cross
  let target : bits(16) = getArchPC() + 2 + sign_extend(16, Offset);
  let crossed = (getArchPC() + 2)[15..8] != target[15..8];
  addCycles(if crossed then 4 else 3);
  setNextPC(target);
  RETIRE_SUCCESS
}

// -----------------------------------------------------------------------------
// STZ - Store Zero
// -----------------------------------------------------------------------------

union clause instruction = STZ_ZeroPage : bits(8)
function clause execute STZ_ZeroPage(Addr) = {
  addCycles(calcCycles(AM_ZeroPage, OC_Write));
  writeMem(0x00 @ Addr, 0x00);
  RETIRE_SUCCESS
}

union clause instruction = STZ_ZeroPageX : bits(8)
function clause execute STZ_ZeroPageX(Addr) = {
  addCycles(calcCycles(AM_ZeroPageX, OC_Write));
  writeMem(0x00 @ ((Addr + X) & 0xFF), 0x00);
  RETIRE_SUCCESS
}

union clause instruction = STZ_Absolute : bits(16)
function clause execute STZ_Absolute(Addr) = {
  addCycles(calcCycles(AM_Absolute, OC_Write));
  writeMem(Addr, 0x00);
  RETIRE_SUCCESS
}

union clause instruction = STZ_AbsoluteX : bits(16)
function clause execute STZ_AbsoluteX(Addr) = {
  addCycles(calcCycles(AM_AbsoluteX, OC_Write));
  writeMem(Addr + (0x00 @ X), 0x00);
  RETIRE_SUCCESS
}

// -----------------------------------------------------------------------------
// INC A, DEC A - Accumulator increment/decrement
// -----------------------------------------------------------------------------

union clause instruction = INC_Accumulator : unit
function clause execute INC_Accumulator() = {
  addCycles(calcCycles(AM_Accumulator, OC_RMW));
  A = A + 1;
  setNZ(A);
  RETIRE_SUCCESS
}

union clause instruction = DEC_Accumulator : unit
function clause execute DEC_Accumulator() = {
  addCycles(calcCycles(AM_Accumulator, OC_RMW));
  A = A - 1;
  setNZ(A);
  RETIRE_SUCCESS
}

// -----------------------------------------------------------------------------
// TRB - Test and Reset Bits
// -----------------------------------------------------------------------------

val doTRB : bits(16) -> unit
function doTRB(Ea) = {
  let Value = readMem(Ea);
  Z = if (A & Value) == 0x00 then 0b1 else 0b0;
  writeMem(Ea, Value & (A ^ 0xFF))
}

union clause instruction = TRB_ZeroPage : bits(8)
function clause execute TRB_ZeroPage(Addr) = {
  addCycles(calcCycles(AM_ZeroPage, OC_RMW));
  doTRB(0x00 @ Addr);
  RETIRE_SUCCESS
}

union clause instruction = TRB_Absolute : bits(16)
function clause execute TRB_Absolute(Addr) = {
  addCycles(calcCycles(AM_Absolute, OC_RMW));
  doTRB(Addr);
  RETIRE_SUCCESS
}

// -----------------------------------------------------------------------------
// TSB - Test and Set Bits
// -----------------------------------------------------------------------------

val doTSB : bits(16) -> unit
function doTSB(Ea) = {
  let Value = readMem(Ea);
  Z = if (A & Value) == 0x00 then 0b1 else 0b0;
  writeMem(Ea, Value | A)
}

union clause instruction = TSB_ZeroPage : bits(8)
function clause execute TSB_ZeroPage(Addr) = {
  addCycles(calcCycles(AM_ZeroPage, OC_RMW));
  doTSB(0x00 @ Addr);
  RETIRE_SUCCESS
}

union clause instruction = TSB_Absolute : bits(16)
function clause execute TSB_Absolute(Addr) = {
  addCycles(calcCycles(AM_Absolute, OC_RMW));
  doTSB(Addr);
  RETIRE_SUCCESS
}

// -----------------------------------------------------------------------------
// JMP (abs,X) - Indexed Indirect Jump
// -----------------------------------------------------------------------------

// 65C02 fixes the NMOS JMP ($xxFF) bug and adds JMP (abs,X)
let CYCLES_JMP_ABSX : int = 6

union clause instruction = JMP_AbsoluteX : bits(16)
function clause execute JMP_AbsoluteX(Addr) = {
  addCycles(CYCLES_JMP_ABSX);
  let Ea = Addr + (0x00 @ X);
  setNextPC(readMem16(Ea));
  RETIRE_SUCCESS
}

// =============================================================================
// New Addressing Mode: Zero Page Indirect (no index)
// =============================================================================

// The 65C02 adds ($zp) addressing - indirect without X or Y indexing
// This is used by: LDA, STA, ORA, AND, EOR, ADC, SBC, CMP

// Base cycle count for ZP Indirect (no index): 5 cycles read, 5 cycles write
// (Like Indirect,Y but without the index addition)

// Example for LDA ($zp):
// union clause instruction = LDA_ZeroPageIndirect : bits(8)
// function clause execute LDA_ZeroPageIndirect(Addr) = {
//   addCycles(5);
//   let Ea = readMem16(0x00 @ Addr);
//   A = readMem(Ea);
//   setNZ(A);
//   RETIRE_SUCCESS
// }

// TODO: Add all ($zp) addressing mode variants when implementing full 65C02
