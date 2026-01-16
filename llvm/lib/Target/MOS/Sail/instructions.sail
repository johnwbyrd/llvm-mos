// MOS 65xx SAIL Specification - Instructions
// All 6502 instruction definitions and helpers

// =============================================================================
// Instruction AST
// =============================================================================

scattered union instruction

val execute : instruction -> ExecutionResult
scattered function execute

// =============================================================================
// Addressing Mode Enum (for documentation/debugging)
// =============================================================================

enum AddrMode = {
  AM_Implicit,
  AM_Accumulator,
  AM_Immediate,
  AM_ZeroPage,
  AM_ZeroPageX,
  AM_ZeroPageY,
  AM_Absolute,
  AM_AbsoluteX,
  AM_AbsoluteY,
  AM_IndirectX,
  AM_IndirectY,
  AM_Relative,
  AM_Indirect
}

// =============================================================================
// Arithmetic Helpers
// =============================================================================

val doADC : bits(8) -> unit
function doADC(Value) = {
  if D == 0b1 & currentlyEnabled(Ext_Decimal) then {
    // BCD (Decimal) mode
    let Lo : bits(8) = (0x0 @ A[3..0]) + (0x0 @ Value[3..0]) + (0b0000000 @ C);
    let LoAdj : bits(8) = if Lo >_u 0x09 then Lo + 0x06 else Lo;
    let LoCarry : bits(8) = if LoAdj >_u 0x0F then 0x01 else 0x00;
    let Hi : bits(8) = (0x0 @ A[7..4]) + (0x0 @ Value[7..4]) + LoCarry;
    let HiAdj : bits(8) = if Hi >_u 0x09 then Hi + 0x06 else Hi;
    C = if HiAdj >_u 0x0F then 0b1 else 0b0;
    A = HiAdj[3..0] @ LoAdj[3..0];
    setNZ(A)
    // V is undefined in decimal mode on NMOS 6502
  } else {
    // Binary mode
    let CarryIn : bits(9) = 0x00 @ C;
    let Sum : bits(9) = (0b0 @ A) + (0b0 @ Value) + CarryIn;
    let Result : bits(8) = Sum[7..0];
    V = (A[7..7] ^ Result[7..7]) & (Value[7..7] ^ Result[7..7]);
    C = Sum[8..8];
    A = Result;
    setNZ(A)
  }
}

val doSBC : bits(8) -> unit
function doSBC(Value) = {
  if D == 0b1 & currentlyEnabled(Ext_Decimal) then {
    // BCD (Decimal) mode
    let Lo : bits(8) = (0x0 @ A[3..0]) - (0x0 @ Value[3..0]) - (0b0000000 @ (C ^ 0b1));
    let LoBorrow : bits(1) = Lo[4..4];
    let LoAdj : bits(8) = if LoBorrow == 0b1 then Lo - 0x06 else Lo;
    let Hi : bits(8) = (0x0 @ A[7..4]) - (0x0 @ Value[7..4]) - (0b0000000 @ LoBorrow);
    let HiBorrow : bits(1) = Hi[4..4];
    let HiAdj : bits(8) = if HiBorrow == 0b1 then Hi - 0x06 else Hi;
    C = HiBorrow ^ 0b1;
    A = HiAdj[3..0] @ LoAdj[3..0];
    setNZ(A)
    // V is undefined in decimal mode on NMOS 6502
  } else {
    // Binary mode - SBC is ADC with inverted operand
    let BorrowIn : bits(9) = 0x00 @ C;
    let InvValue : bits(8) = Value ^ 0xFF;
    let Diff : bits(9) = (0b0 @ A) + (0b0 @ InvValue) + BorrowIn;
    let Result : bits(8) = Diff[7..0];
    V = (A[7..7] ^ Result[7..7]) & (InvValue[7..7] ^ Result[7..7]);
    C = Diff[8..8];
    A = Result;
    setNZ(A)
  }
}

// =============================================================================
// Compare/Bit Test Helpers
// =============================================================================

val doCMP : (bits(8), bits(8)) -> unit
function doCMP(Reg, Value) = {
  let Result : bits(9) = (0b0 @ Reg) - (0b0 @ Value);
  N = Result[7..7];
  Z = if Result[7..0] == 0x00 then 0b1 else 0b0;
  C = if Reg >=_u Value then 0b1 else 0b0
}

val doBIT : bits(8) -> unit
function doBIT(Value) = {
  N = Value[7..7];
  V = Value[6..6];
  Z = if (A & Value) == 0x00 then 0b1 else 0b0
}

// =============================================================================
// Memory Read-Modify-Write Helpers
// =============================================================================

val doIncMem : bits(16) -> unit
function doIncMem(Ea) = {
  let Result = readMem(Ea) + 1;
  writeMem(Ea, Result);
  setNZ(Result)
}

val doDecMem : bits(16) -> unit
function doDecMem(Ea) = {
  let Result = readMem(Ea) - 1;
  writeMem(Ea, Result);
  setNZ(Result)
}

val doASLMem : bits(16) -> unit
function doASLMem(Ea) = {
  let Value = readMem(Ea);
  C = Value[7..7];
  let Result = Value << 1;
  writeMem(Ea, Result);
  setNZ(Result)
}

val doLSRMem : bits(16) -> unit
function doLSRMem(Ea) = {
  let Value = readMem(Ea);
  C = Value[0..0];
  let Result = Value >> 1;
  writeMem(Ea, Result);
  setNZ(Result)
}

val doROLMem : bits(16) -> unit
function doROLMem(Ea) = {
  let Value = readMem(Ea);
  let OldCarry = C;
  C = Value[7..7];
  let Result : bits(8) = (Value[6..0] @ OldCarry);
  writeMem(Ea, Result);
  setNZ(Result)
}

val doRORMem : bits(16) -> unit
function doRORMem(Ea) = {
  let Value = readMem(Ea);
  let OldCarry = C;
  C = Value[0..0];
  let Result : bits(8) = (OldCarry @ Value[7..1]);
  writeMem(Ea, Result);
  setNZ(Result)
}

// =============================================================================
// Branch Helper
// =============================================================================

val doBranch : (bool, bits(8)) -> ExecutionResult
function doBranch(Cond, Offset) = {
  if Cond then setNextPC(NextPC + sign_extend(Offset));
  RETIRE_SUCCESS
}

// =============================================================================
// LDA - Load Accumulator
// =============================================================================

union clause instruction = LDA_Immediate : bits(8)
union clause instruction = LDA_ZeroPage : bits(8)
union clause instruction = LDA_ZeroPageX : bits(8)
union clause instruction = LDA_Absolute : bits(16)
union clause instruction = LDA_AbsoluteX : bits(16)
union clause instruction = LDA_AbsoluteY : bits(16)
union clause instruction = LDA_IndexedIndirect : bits(8)
union clause instruction = LDA_IndirectIndexed : bits(8)

function clause execute LDA_Immediate(Imm) = { A = Imm; setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_ZeroPage(Addr) = { A = readMem(0x00 @ Addr); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_ZeroPageX(Addr) = { A = readMem(0x00 @ (Addr + X)); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_Absolute(Addr) = { A = readMem(Addr); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_AbsoluteX(Addr) = { A = readMem(Addr + (0x00 @ X)); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_AbsoluteY(Addr) = { A = readMem(Addr + (0x00 @ Y)); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_IndexedIndirect(Addr) = { let Ea = readMem16(0x00 @ (Addr + X)); A = readMem(Ea); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_IndirectIndexed(Addr) = { let Ea = readMem16(0x00 @ Addr) + (0x00 @ Y); A = readMem(Ea); setNZ(A); RETIRE_SUCCESS }

// =============================================================================
// LDX - Load X Register
// =============================================================================

union clause instruction = LDX_Immediate : bits(8)
union clause instruction = LDX_ZeroPage : bits(8)
union clause instruction = LDX_ZeroPageY : bits(8)
union clause instruction = LDX_Absolute : bits(16)
union clause instruction = LDX_AbsoluteY : bits(16)

function clause execute LDX_Immediate(Imm) = { X = Imm; setNZ(X); RETIRE_SUCCESS }
function clause execute LDX_ZeroPage(Addr) = { X = readMem(0x00 @ Addr); setNZ(X); RETIRE_SUCCESS }
function clause execute LDX_ZeroPageY(Addr) = { X = readMem(0x00 @ (Addr + Y)); setNZ(X); RETIRE_SUCCESS }
function clause execute LDX_Absolute(Addr) = { X = readMem(Addr); setNZ(X); RETIRE_SUCCESS }
function clause execute LDX_AbsoluteY(Addr) = { X = readMem(Addr + (0x00 @ Y)); setNZ(X); RETIRE_SUCCESS }

// =============================================================================
// LDY - Load Y Register
// =============================================================================

union clause instruction = LDY_Immediate : bits(8)
union clause instruction = LDY_ZeroPage : bits(8)
union clause instruction = LDY_ZeroPageX : bits(8)
union clause instruction = LDY_Absolute : bits(16)
union clause instruction = LDY_AbsoluteX : bits(16)

function clause execute LDY_Immediate(Imm) = { Y = Imm; setNZ(Y); RETIRE_SUCCESS }
function clause execute LDY_ZeroPage(Addr) = { Y = readMem(0x00 @ Addr); setNZ(Y); RETIRE_SUCCESS }
function clause execute LDY_ZeroPageX(Addr) = { Y = readMem(0x00 @ (Addr + X)); setNZ(Y); RETIRE_SUCCESS }
function clause execute LDY_Absolute(Addr) = { Y = readMem(Addr); setNZ(Y); RETIRE_SUCCESS }
function clause execute LDY_AbsoluteX(Addr) = { Y = readMem(Addr + (0x00 @ X)); setNZ(Y); RETIRE_SUCCESS }

// =============================================================================
// STA - Store Accumulator
// =============================================================================

union clause instruction = STA_ZeroPage : bits(8)
union clause instruction = STA_ZeroPageX : bits(8)
union clause instruction = STA_Absolute : bits(16)
union clause instruction = STA_AbsoluteX : bits(16)
union clause instruction = STA_AbsoluteY : bits(16)
union clause instruction = STA_IndexedIndirect : bits(8)
union clause instruction = STA_IndirectIndexed : bits(8)

function clause execute STA_ZeroPage(Addr) = { writeMem(0x00 @ Addr, A); RETIRE_SUCCESS }
function clause execute STA_ZeroPageX(Addr) = { writeMem(0x00 @ (Addr + X), A); RETIRE_SUCCESS }
function clause execute STA_Absolute(Addr) = { writeMem(Addr, A); RETIRE_SUCCESS }
function clause execute STA_AbsoluteX(Addr) = { writeMem(Addr + (0x00 @ X), A); RETIRE_SUCCESS }
function clause execute STA_AbsoluteY(Addr) = { writeMem(Addr + (0x00 @ Y), A); RETIRE_SUCCESS }
function clause execute STA_IndexedIndirect(Addr) = { let Ea = readMem16(0x00 @ (Addr + X)); writeMem(Ea, A); RETIRE_SUCCESS }
function clause execute STA_IndirectIndexed(Addr) = { let Ea = readMem16(0x00 @ Addr) + (0x00 @ Y); writeMem(Ea, A); RETIRE_SUCCESS }

// =============================================================================
// STX - Store X Register
// =============================================================================

union clause instruction = STX_ZeroPage : bits(8)
union clause instruction = STX_ZeroPageY : bits(8)
union clause instruction = STX_Absolute : bits(16)

function clause execute STX_ZeroPage(Addr) = { writeMem(0x00 @ Addr, X); RETIRE_SUCCESS }
function clause execute STX_ZeroPageY(Addr) = { writeMem(0x00 @ (Addr + Y), X); RETIRE_SUCCESS }
function clause execute STX_Absolute(Addr) = { writeMem(Addr, X); RETIRE_SUCCESS }

// =============================================================================
// STY - Store Y Register
// =============================================================================

union clause instruction = STY_ZeroPage : bits(8)
union clause instruction = STY_ZeroPageX : bits(8)
union clause instruction = STY_Absolute : bits(16)

function clause execute STY_ZeroPage(Addr) = { writeMem(0x00 @ Addr, Y); RETIRE_SUCCESS }
function clause execute STY_ZeroPageX(Addr) = { writeMem(0x00 @ (Addr + X), Y); RETIRE_SUCCESS }
function clause execute STY_Absolute(Addr) = { writeMem(Addr, Y); RETIRE_SUCCESS }

// =============================================================================
// ADC - Add with Carry
// =============================================================================

union clause instruction = ADC_Immediate : bits(8)
union clause instruction = ADC_ZeroPage : bits(8)
union clause instruction = ADC_ZeroPageX : bits(8)
union clause instruction = ADC_Absolute : bits(16)
union clause instruction = ADC_AbsoluteX : bits(16)
union clause instruction = ADC_AbsoluteY : bits(16)
union clause instruction = ADC_IndexedIndirect : bits(8)
union clause instruction = ADC_IndirectIndexed : bits(8)

function clause execute ADC_Immediate(Imm) = { doADC(Imm); RETIRE_SUCCESS }
function clause execute ADC_ZeroPage(Addr) = { doADC(readMem(0x00 @ Addr)); RETIRE_SUCCESS }
function clause execute ADC_ZeroPageX(Addr) = { doADC(readMem(0x00 @ (Addr + X))); RETIRE_SUCCESS }
function clause execute ADC_Absolute(Addr) = { doADC(readMem(Addr)); RETIRE_SUCCESS }
function clause execute ADC_AbsoluteX(Addr) = { doADC(readMem(Addr + (0x00 @ X))); RETIRE_SUCCESS }
function clause execute ADC_AbsoluteY(Addr) = { doADC(readMem(Addr + (0x00 @ Y))); RETIRE_SUCCESS }
function clause execute ADC_IndexedIndirect(Addr) = { let Ea = readMem16(0x00 @ (Addr + X)); doADC(readMem(Ea)); RETIRE_SUCCESS }
function clause execute ADC_IndirectIndexed(Addr) = { let Ea = readMem16(0x00 @ Addr) + (0x00 @ Y); doADC(readMem(Ea)); RETIRE_SUCCESS }

// =============================================================================
// SBC - Subtract with Carry
// =============================================================================

union clause instruction = SBC_Immediate : bits(8)
union clause instruction = SBC_ZeroPage : bits(8)
union clause instruction = SBC_ZeroPageX : bits(8)
union clause instruction = SBC_Absolute : bits(16)
union clause instruction = SBC_AbsoluteX : bits(16)
union clause instruction = SBC_AbsoluteY : bits(16)
union clause instruction = SBC_IndexedIndirect : bits(8)
union clause instruction = SBC_IndirectIndexed : bits(8)

function clause execute SBC_Immediate(Imm) = { doSBC(Imm); RETIRE_SUCCESS }
function clause execute SBC_ZeroPage(Addr) = { doSBC(readMem(0x00 @ Addr)); RETIRE_SUCCESS }
function clause execute SBC_ZeroPageX(Addr) = { doSBC(readMem(0x00 @ (Addr + X))); RETIRE_SUCCESS }
function clause execute SBC_Absolute(Addr) = { doSBC(readMem(Addr)); RETIRE_SUCCESS }
function clause execute SBC_AbsoluteX(Addr) = { doSBC(readMem(Addr + (0x00 @ X))); RETIRE_SUCCESS }
function clause execute SBC_AbsoluteY(Addr) = { doSBC(readMem(Addr + (0x00 @ Y))); RETIRE_SUCCESS }
function clause execute SBC_IndexedIndirect(Addr) = { let Ea = readMem16(0x00 @ (Addr + X)); doSBC(readMem(Ea)); RETIRE_SUCCESS }
function clause execute SBC_IndirectIndexed(Addr) = { let Ea = readMem16(0x00 @ Addr) + (0x00 @ Y); doSBC(readMem(Ea)); RETIRE_SUCCESS }

// =============================================================================
// AND - Logical AND
// =============================================================================

union clause instruction = AND_Immediate : bits(8)
union clause instruction = AND_ZeroPage : bits(8)
union clause instruction = AND_ZeroPageX : bits(8)
union clause instruction = AND_Absolute : bits(16)
union clause instruction = AND_AbsoluteX : bits(16)
union clause instruction = AND_AbsoluteY : bits(16)
union clause instruction = AND_IndexedIndirect : bits(8)
union clause instruction = AND_IndirectIndexed : bits(8)

function clause execute AND_Immediate(Imm) = { A = A & Imm; setNZ(A); RETIRE_SUCCESS }
function clause execute AND_ZeroPage(Addr) = { A = A & readMem(0x00 @ Addr); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_ZeroPageX(Addr) = { A = A & readMem(0x00 @ (Addr + X)); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_Absolute(Addr) = { A = A & readMem(Addr); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_AbsoluteX(Addr) = { A = A & readMem(Addr + (0x00 @ X)); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_AbsoluteY(Addr) = { A = A & readMem(Addr + (0x00 @ Y)); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_IndexedIndirect(Addr) = { let Ea = readMem16(0x00 @ (Addr + X)); A = A & readMem(Ea); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_IndirectIndexed(Addr) = { let Ea = readMem16(0x00 @ Addr) + (0x00 @ Y); A = A & readMem(Ea); setNZ(A); RETIRE_SUCCESS }

// =============================================================================
// ORA - Logical OR
// =============================================================================

union clause instruction = ORA_Immediate : bits(8)
union clause instruction = ORA_ZeroPage : bits(8)
union clause instruction = ORA_ZeroPageX : bits(8)
union clause instruction = ORA_Absolute : bits(16)
union clause instruction = ORA_AbsoluteX : bits(16)
union clause instruction = ORA_AbsoluteY : bits(16)
union clause instruction = ORA_IndexedIndirect : bits(8)
union clause instruction = ORA_IndirectIndexed : bits(8)

function clause execute ORA_Immediate(Imm) = { A = A | Imm; setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_ZeroPage(Addr) = { A = A | readMem(0x00 @ Addr); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_ZeroPageX(Addr) = { A = A | readMem(0x00 @ (Addr + X)); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_Absolute(Addr) = { A = A | readMem(Addr); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_AbsoluteX(Addr) = { A = A | readMem(Addr + (0x00 @ X)); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_AbsoluteY(Addr) = { A = A | readMem(Addr + (0x00 @ Y)); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_IndexedIndirect(Addr) = { let Ea = readMem16(0x00 @ (Addr + X)); A = A | readMem(Ea); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_IndirectIndexed(Addr) = { let Ea = readMem16(0x00 @ Addr) + (0x00 @ Y); A = A | readMem(Ea); setNZ(A); RETIRE_SUCCESS }

// =============================================================================
// EOR - Logical Exclusive OR
// =============================================================================

union clause instruction = EOR_Immediate : bits(8)
union clause instruction = EOR_ZeroPage : bits(8)
union clause instruction = EOR_ZeroPageX : bits(8)
union clause instruction = EOR_Absolute : bits(16)
union clause instruction = EOR_AbsoluteX : bits(16)
union clause instruction = EOR_AbsoluteY : bits(16)
union clause instruction = EOR_IndexedIndirect : bits(8)
union clause instruction = EOR_IndirectIndexed : bits(8)

function clause execute EOR_Immediate(Imm) = { A = A ^ Imm; setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_ZeroPage(Addr) = { A = A ^ readMem(0x00 @ Addr); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_ZeroPageX(Addr) = { A = A ^ readMem(0x00 @ (Addr + X)); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_Absolute(Addr) = { A = A ^ readMem(Addr); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_AbsoluteX(Addr) = { A = A ^ readMem(Addr + (0x00 @ X)); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_AbsoluteY(Addr) = { A = A ^ readMem(Addr + (0x00 @ Y)); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_IndexedIndirect(Addr) = { let Ea = readMem16(0x00 @ (Addr + X)); A = A ^ readMem(Ea); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_IndirectIndexed(Addr) = { let Ea = readMem16(0x00 @ Addr) + (0x00 @ Y); A = A ^ readMem(Ea); setNZ(A); RETIRE_SUCCESS }

// =============================================================================
// BIT - Bit Test
// =============================================================================

union clause instruction = BIT_ZeroPage : bits(8)
union clause instruction = BIT_Absolute : bits(16)

function clause execute BIT_ZeroPage(Addr) = { doBIT(readMem(0x00 @ Addr)); RETIRE_SUCCESS }
function clause execute BIT_Absolute(Addr) = { doBIT(readMem(Addr)); RETIRE_SUCCESS }

// =============================================================================
// CMP - Compare Accumulator
// =============================================================================

union clause instruction = CMP_Immediate : bits(8)
union clause instruction = CMP_ZeroPage : bits(8)
union clause instruction = CMP_ZeroPageX : bits(8)
union clause instruction = CMP_Absolute : bits(16)
union clause instruction = CMP_AbsoluteX : bits(16)
union clause instruction = CMP_AbsoluteY : bits(16)
union clause instruction = CMP_IndexedIndirect : bits(8)
union clause instruction = CMP_IndirectIndexed : bits(8)

function clause execute CMP_Immediate(Imm) = { doCMP(A, Imm); RETIRE_SUCCESS }
function clause execute CMP_ZeroPage(Addr) = { doCMP(A, readMem(0x00 @ Addr)); RETIRE_SUCCESS }
function clause execute CMP_ZeroPageX(Addr) = { doCMP(A, readMem(0x00 @ (Addr + X))); RETIRE_SUCCESS }
function clause execute CMP_Absolute(Addr) = { doCMP(A, readMem(Addr)); RETIRE_SUCCESS }
function clause execute CMP_AbsoluteX(Addr) = { doCMP(A, readMem(Addr + (0x00 @ X))); RETIRE_SUCCESS }
function clause execute CMP_AbsoluteY(Addr) = { doCMP(A, readMem(Addr + (0x00 @ Y))); RETIRE_SUCCESS }
function clause execute CMP_IndexedIndirect(Addr) = { let Ea = readMem16(0x00 @ (Addr + X)); doCMP(A, readMem(Ea)); RETIRE_SUCCESS }
function clause execute CMP_IndirectIndexed(Addr) = { let Ea = readMem16(0x00 @ Addr) + (0x00 @ Y); doCMP(A, readMem(Ea)); RETIRE_SUCCESS }

// =============================================================================
// CPX - Compare X Register
// =============================================================================

union clause instruction = CPX_Immediate : bits(8)
union clause instruction = CPX_ZeroPage : bits(8)
union clause instruction = CPX_Absolute : bits(16)

function clause execute CPX_Immediate(Imm) = { doCMP(X, Imm); RETIRE_SUCCESS }
function clause execute CPX_ZeroPage(Addr) = { doCMP(X, readMem(0x00 @ Addr)); RETIRE_SUCCESS }
function clause execute CPX_Absolute(Addr) = { doCMP(X, readMem(Addr)); RETIRE_SUCCESS }

// =============================================================================
// CPY - Compare Y Register
// =============================================================================

union clause instruction = CPY_Immediate : bits(8)
union clause instruction = CPY_ZeroPage : bits(8)
union clause instruction = CPY_Absolute : bits(16)

function clause execute CPY_Immediate(Imm) = { doCMP(Y, Imm); RETIRE_SUCCESS }
function clause execute CPY_ZeroPage(Addr) = { doCMP(Y, readMem(0x00 @ Addr)); RETIRE_SUCCESS }
function clause execute CPY_Absolute(Addr) = { doCMP(Y, readMem(Addr)); RETIRE_SUCCESS }

// =============================================================================
// INC - Increment Memory
// =============================================================================

union clause instruction = INC_ZeroPage : bits(8)
union clause instruction = INC_ZeroPageX : bits(8)
union clause instruction = INC_Absolute : bits(16)
union clause instruction = INC_AbsoluteX : bits(16)

function clause execute INC_ZeroPage(Addr) = { doIncMem(0x00 @ Addr); RETIRE_SUCCESS }
function clause execute INC_ZeroPageX(Addr) = { doIncMem(0x00 @ (Addr + X)); RETIRE_SUCCESS }
function clause execute INC_Absolute(Addr) = { doIncMem(Addr); RETIRE_SUCCESS }
function clause execute INC_AbsoluteX(Addr) = { doIncMem(Addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// DEC - Decrement Memory
// =============================================================================

union clause instruction = DEC_ZeroPage : bits(8)
union clause instruction = DEC_ZeroPageX : bits(8)
union clause instruction = DEC_Absolute : bits(16)
union clause instruction = DEC_AbsoluteX : bits(16)

function clause execute DEC_ZeroPage(Addr) = { doDecMem(0x00 @ Addr); RETIRE_SUCCESS }
function clause execute DEC_ZeroPageX(Addr) = { doDecMem(0x00 @ (Addr + X)); RETIRE_SUCCESS }
function clause execute DEC_Absolute(Addr) = { doDecMem(Addr); RETIRE_SUCCESS }
function clause execute DEC_AbsoluteX(Addr) = { doDecMem(Addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// INX, INY, DEX, DEY - Register Increment/Decrement
// =============================================================================

union clause instruction = INX_Implied : unit
union clause instruction = INY_Implied : unit
union clause instruction = DEX_Implied : unit
union clause instruction = DEY_Implied : unit

function clause execute INX_Implied() = { X = X + 1; setNZ(X); RETIRE_SUCCESS }
function clause execute INY_Implied() = { Y = Y + 1; setNZ(Y); RETIRE_SUCCESS }
function clause execute DEX_Implied() = { X = X - 1; setNZ(X); RETIRE_SUCCESS }
function clause execute DEY_Implied() = { Y = Y - 1; setNZ(Y); RETIRE_SUCCESS }

// =============================================================================
// ASL - Arithmetic Shift Left
// =============================================================================

union clause instruction = ASL_Accumulator : unit
union clause instruction = ASL_ZeroPage : bits(8)
union clause instruction = ASL_ZeroPageX : bits(8)
union clause instruction = ASL_Absolute : bits(16)
union clause instruction = ASL_AbsoluteX : bits(16)

function clause execute ASL_Accumulator() = { C = A[7..7]; A = A << 1; setNZ(A); RETIRE_SUCCESS }
function clause execute ASL_ZeroPage(Addr) = { doASLMem(0x00 @ Addr); RETIRE_SUCCESS }
function clause execute ASL_ZeroPageX(Addr) = { doASLMem(0x00 @ (Addr + X)); RETIRE_SUCCESS }
function clause execute ASL_Absolute(Addr) = { doASLMem(Addr); RETIRE_SUCCESS }
function clause execute ASL_AbsoluteX(Addr) = { doASLMem(Addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// LSR - Logical Shift Right
// =============================================================================

union clause instruction = LSR_Accumulator : unit
union clause instruction = LSR_ZeroPage : bits(8)
union clause instruction = LSR_ZeroPageX : bits(8)
union clause instruction = LSR_Absolute : bits(16)
union clause instruction = LSR_AbsoluteX : bits(16)

function clause execute LSR_Accumulator() = { C = A[0..0]; A = A >> 1; setNZ(A); RETIRE_SUCCESS }
function clause execute LSR_ZeroPage(Addr) = { doLSRMem(0x00 @ Addr); RETIRE_SUCCESS }
function clause execute LSR_ZeroPageX(Addr) = { doLSRMem(0x00 @ (Addr + X)); RETIRE_SUCCESS }
function clause execute LSR_Absolute(Addr) = { doLSRMem(Addr); RETIRE_SUCCESS }
function clause execute LSR_AbsoluteX(Addr) = { doLSRMem(Addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// ROL - Rotate Left
// =============================================================================

union clause instruction = ROL_Accumulator : unit
union clause instruction = ROL_ZeroPage : bits(8)
union clause instruction = ROL_ZeroPageX : bits(8)
union clause instruction = ROL_Absolute : bits(16)
union clause instruction = ROL_AbsoluteX : bits(16)

function clause execute ROL_Accumulator() = { let OldC = C; C = A[7..7]; A = A[6..0] @ OldC; setNZ(A); RETIRE_SUCCESS }
function clause execute ROL_ZeroPage(Addr) = { doROLMem(0x00 @ Addr); RETIRE_SUCCESS }
function clause execute ROL_ZeroPageX(Addr) = { doROLMem(0x00 @ (Addr + X)); RETIRE_SUCCESS }
function clause execute ROL_Absolute(Addr) = { doROLMem(Addr); RETIRE_SUCCESS }
function clause execute ROL_AbsoluteX(Addr) = { doROLMem(Addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// ROR - Rotate Right
// =============================================================================

union clause instruction = ROR_Accumulator : unit
union clause instruction = ROR_ZeroPage : bits(8)
union clause instruction = ROR_ZeroPageX : bits(8)
union clause instruction = ROR_Absolute : bits(16)
union clause instruction = ROR_AbsoluteX : bits(16)

function clause execute ROR_Accumulator() = { let OldC = C; C = A[0..0]; A = OldC @ A[7..1]; setNZ(A); RETIRE_SUCCESS }
function clause execute ROR_ZeroPage(Addr) = { doRORMem(0x00 @ Addr); RETIRE_SUCCESS }
function clause execute ROR_ZeroPageX(Addr) = { doRORMem(0x00 @ (Addr + X)); RETIRE_SUCCESS }
function clause execute ROR_Absolute(Addr) = { doRORMem(Addr); RETIRE_SUCCESS }
function clause execute ROR_AbsoluteX(Addr) = { doRORMem(Addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// Transfer Instructions
// =============================================================================

union clause instruction = TAX_Implied : unit
union clause instruction = TXA_Implied : unit
union clause instruction = TAY_Implied : unit
union clause instruction = TYA_Implied : unit
union clause instruction = TSX_Implied : unit
union clause instruction = TXS_Implied : unit

function clause execute TAX_Implied() = { X = A; setNZ(X); RETIRE_SUCCESS }
function clause execute TXA_Implied() = { A = X; setNZ(A); RETIRE_SUCCESS }
function clause execute TAY_Implied() = { Y = A; setNZ(Y); RETIRE_SUCCESS }
function clause execute TYA_Implied() = { A = Y; setNZ(A); RETIRE_SUCCESS }
function clause execute TSX_Implied() = { X = S; setNZ(X); RETIRE_SUCCESS }
function clause execute TXS_Implied() = { S = X; RETIRE_SUCCESS }  // Does NOT affect flags

// =============================================================================
// Stack Instructions
// =============================================================================

union clause instruction = PHA_Implied : unit
union clause instruction = PLA_Implied : unit
union clause instruction = PHP_Implied : unit
union clause instruction = PLP_Implied : unit

function clause execute PHA_Implied() = { push(A); RETIRE_SUCCESS }
function clause execute PLA_Implied() = { A = pull(); setNZ(A); RETIRE_SUCCESS }
function clause execute PHP_Implied() = { push(getP()); RETIRE_SUCCESS }
function clause execute PLP_Implied() = { setP(pull()); RETIRE_SUCCESS }

// =============================================================================
// Flag Instructions
// =============================================================================

union clause instruction = CLC_Implied : unit
union clause instruction = SEC_Implied : unit
union clause instruction = CLI_Implied : unit
union clause instruction = SEI_Implied : unit
union clause instruction = CLD_Implied : unit
union clause instruction = SED_Implied : unit
union clause instruction = CLV_Implied : unit

function clause execute CLC_Implied() = { C = 0b0; RETIRE_SUCCESS }
function clause execute SEC_Implied() = { C = 0b1; RETIRE_SUCCESS }
function clause execute CLI_Implied() = { I = 0b0; RETIRE_SUCCESS }
function clause execute SEI_Implied() = { I = 0b1; RETIRE_SUCCESS }
function clause execute CLD_Implied() = { D = 0b0; RETIRE_SUCCESS }
function clause execute SED_Implied() = { D = 0b1; RETIRE_SUCCESS }
function clause execute CLV_Implied() = { V = 0b0; RETIRE_SUCCESS }

// =============================================================================
// Branch Instructions
// =============================================================================

union clause instruction = BPL_Relative : bits(8)
union clause instruction = BMI_Relative : bits(8)
union clause instruction = BVC_Relative : bits(8)
union clause instruction = BVS_Relative : bits(8)
union clause instruction = BCC_Relative : bits(8)
union clause instruction = BCS_Relative : bits(8)
union clause instruction = BNE_Relative : bits(8)
union clause instruction = BEQ_Relative : bits(8)

function clause execute BPL_Relative(Offset) = doBranch(N == 0b0, Offset)
function clause execute BMI_Relative(Offset) = doBranch(N == 0b1, Offset)
function clause execute BVC_Relative(Offset) = doBranch(V == 0b0, Offset)
function clause execute BVS_Relative(Offset) = doBranch(V == 0b1, Offset)
function clause execute BCC_Relative(Offset) = doBranch(C == 0b0, Offset)
function clause execute BCS_Relative(Offset) = doBranch(C == 0b1, Offset)
function clause execute BNE_Relative(Offset) = doBranch(Z == 0b0, Offset)
function clause execute BEQ_Relative(Offset) = doBranch(Z == 0b1, Offset)

// =============================================================================
// Jump Instructions
// =============================================================================

union clause instruction = JMP_Absolute : bits(16)
union clause instruction = JMP_Indirect16 : bits(16)

function clause execute JMP_Absolute(Addr) = { setNextPC(Addr); RETIRE_SUCCESS }

// JMP indirect - 6502 has page-wrap bug, 65C02 fixes it
function clause execute JMP_Indirect16(Addr) = {
  let Lo = readMem(Addr);
  let HiAddr : bits(16) =
    if currentlyEnabled(Ext_65C02) then
      Addr + 1  // 65C02: correct behavior
    else
      Addr[15..8] @ ((Addr[7..0]) + 1);  // 6502: page-wrap bug
  let Hi = readMem(HiAddr);
  setNextPC(Hi @ Lo);
  RETIRE_SUCCESS
}

// =============================================================================
// Subroutine Instructions
// =============================================================================

union clause instruction = JSR_Absolute : bits(16)
union clause instruction = RTS_Implied : unit
union clause instruction = RTI_Implied : unit

function clause execute JSR_Absolute(Addr) = {
  // Push return address - 1 (pointing to last byte of JSR instruction)
  let Ret = NextPC - 1;
  push16(Ret);
  setNextPC(Addr);
  RETIRE_SUCCESS
}

function clause execute RTS_Implied() = {
  setNextPC(pull16() + 1);
  RETIRE_SUCCESS
}

function clause execute RTI_Implied() = {
  setP(pull());
  setNextPC(pull16());
  RETIRE_SUCCESS
}

// =============================================================================
// Misc Instructions
// =============================================================================

union clause instruction = BRK_Implied : unit
union clause instruction = NOP_Implied : unit

function clause execute BRK_Implied() = {
  // Push NextPC (which is PC+2, skipping BRK + padding byte)
  push16(NextPC);
  // Push P with B flag set
  push(getP() | 0x10);
  I = 0b1;
  // Load IRQ vector
  setNextPC(readMem16(0xFFFE));
  RETIRE_SUCCESS
}

function clause execute NOP_Implied() = { RETIRE_SUCCESS }

// =============================================================================
// End scattered union
// =============================================================================

end instruction
end execute
end hardwareCapability
end currentlyEnabled
