// MOS 6502 SAIL Specification
// Formal semantics for the original 6502 instruction set
//
// Future variants (65C02, 65816, etc.) will be separate files that
// extend or modify these definitions.

default Order dec

$include <prelude.sail>
$include <smt.sail>
$include <option.sail>
$include <arith.sail>
$include <vector_dec.sail>

// =============================================================================
// Operators (following sail-riscv conventions)
// =============================================================================

// Bitwise operators
overload operator & = {and_vec}
overload operator | = {or_vec}
overload operator ^ = {xor_vec}

// Arithmetic on bitvectors
val sub_vec = pure {c: "sub_bits", _: "sub_vec"} : forall 'n. (bits('n), bits('n)) -> bits('n)
val sub_vec_int = pure {c: "sub_bits_int", _: "sub_vec_int"} : forall 'n. (bits('n), int) -> bits('n)
val add_vec = pure {c: "add_bits", _: "add_vec"} : forall 'n. (bits('n), bits('n)) -> bits('n)
val add_vec_int = pure {c: "add_bits_int", _: "add_vec_int"} : forall 'n. (bits('n), int) -> bits('n)

overload operator - = {sub_vec, sub_vec_int}
overload operator + = {add_vec, add_vec_int}

// Shift operators
infix 7 >>
infix 7 <<

val "shift_bits_right" : forall 'n 'm. (bits('n), bits('m)) -> bits('n)
val "shift_bits_left"  : forall 'n 'm. (bits('n), bits('m)) -> bits('n)

val "shiftl" : forall 'm 'n, 'n >= 0. (bits('m), int('n)) -> bits('m)
val "shiftr" : forall 'm 'n, 'n >= 0. (bits('m), int('n)) -> bits('m)

overload operator >> = {shift_bits_right, shiftr}
overload operator << = {shift_bits_left, shiftl}

// Unsigned comparison operators for bitvectors
infix 4 >=_u
infix 4 <=_u
infix 4 >_u
infix 4 <_u

val operator >=_u : forall 'n. (bits('n), bits('n)) -> bool
val operator <=_u : forall 'n. (bits('n), bits('n)) -> bool
val operator >_u  : forall 'n. (bits('n), bits('n)) -> bool
val operator <_u  : forall 'n. (bits('n), bits('n)) -> bool

function operator >=_u (x, y) = unsigned(x) >= unsigned(y)
function operator <=_u (x, y) = unsigned(x) <= unsigned(y)
function operator >_u  (x, y) = unsigned(x) > unsigned(y)
function operator <_u  (x, y) = unsigned(x) < unsigned(y)

// Sign/zero extension
val sign_extend : forall 'n 'm, 'm >= 'n. (implicit('m), bits('n)) -> bits('m)
val zero_extend : forall 'n 'm, 'm >= 'n. (implicit('m), bits('n)) -> bits('m)

function sign_extend(m, x) = sail_sign_extend(x, m)
function zero_extend(m, x) = sail_zero_extend(x, m)

// =============================================================================
// Execution Result
// =============================================================================

enum ExecutionResult = { RETIRE_SUCCESS }

// =============================================================================
// Registers
// =============================================================================

register A : bits(8)    // Accumulator
register X : bits(8)    // X index register
register Y : bits(8)    // Y index register
register S : bits(8)    // Stack pointer
register PC : bits(16)  // Program counter
register NextPC : bits(16)  // Next PC (set before execute, may be overridden by branches/jumps)

// Status flags (P register bits)
register N : bits(1)    // Negative (bit 7)
register V : bits(1)    // Overflow (bit 6)
register D : bits(1)    // Decimal mode (bit 3)
register I : bits(1)    // Interrupt disable (bit 2)
register Z : bits(1)    // Zero (bit 1)
register C : bits(1)    // Carry (bit 0)

// Interrupt state (directly set by external hardware/devices)
register IRQPending : bits(1)  // Hardware IRQ line asserted
register NMIPending : bits(1)  // Non-maskable interrupt pending

// =============================================================================
// Memory Interface
// =============================================================================

val readMem : bits(16) -> bits(8)
val writeMem : (bits(16), bits(8)) -> unit

// Read 16-bit value (little-endian)
val readMem16 : bits(16) -> bits(16)
function readMem16(addr) = {
    let lo = readMem(addr);
    let hi = readMem(addr + 1);
    hi @ lo
}

// =============================================================================
// PC Access (following sail-riscv pattern)
// =============================================================================

// Get the current PC
val getArchPC : unit -> bits(16)
function getArchPC() = PC

// Get the next PC (default: PC + instruction size, but branches/jumps override)
val getNextPC : unit -> bits(16)
function getNextPC() = NextPC

// Set the next PC (called by branches and jumps to override default)
val setNextPC : bits(16) -> unit
function setNextPC(pc) = {
    NextPC = pc
}

// Commit NextPC to PC (called after instruction execution)
val tickPC : unit -> unit
function tickPC() = {
    PC = NextPC
}

// =============================================================================
// Addressing Modes
// =============================================================================

enum AddrMode = {
    Implicit,       // No operand (e.g., INX)
    Accumulator,    // Operates on A (e.g., ASL A)
    Immediate,      // #$nn
    ZeroPage,       // $nn
    ZeroPageX,      // $nn,X
    ZeroPageY,      // $nn,Y
    Absolute,       // $nnnn
    AbsoluteX,      // $nnnn,X
    AbsoluteY,      // $nnnn,Y
    IndirectX,      // ($nn,X)
    IndirectY,      // ($nn),Y
    Relative,       // Branch offset
    Indirect        // ($nnnn) - JMP only
}

// =============================================================================
// Flag Helpers
// =============================================================================

val setNZ : bits(8) -> unit
function setNZ(result) = {
    N = result[7..7];
    Z = if result == 0x00 then 0b1 else 0b0
}

val getP : unit -> bits(8)
function getP() = N @ V @ 0b11 @ D @ I @ Z @ C

val setP : bits(8) -> unit
function setP(p) = {
    N = p[7..7];
    V = p[6..6];
    D = p[3..3];
    I = p[2..2];
    Z = p[1..1];
    C = p[0..0]
}

// =============================================================================
// Stack Operations
// =============================================================================

val push : bits(8) -> unit
function push(value) = {
    writeMem(0x01 @ S, value);
    S = S - 1
}

val pull : unit -> bits(8)
function pull() = {
    S = S + 1;
    readMem(0x01 @ S)
}

val push16 : bits(16) -> unit
function push16(value) = {
    push(value[15..8]);
    push(value[7..0])
}

val pull16 : unit -> bits(16)
function pull16() = {
    let lo = pull();
    let hi = pull();
    hi @ lo
}

// =============================================================================
// Interrupt Handling
// =============================================================================

// IRQ vector addresses
let IrqVector : bits(16) = 0xFFFE
let NmiVector : bits(16) = 0xFFFA

// Check for and handle pending IRQ.
// Returns true if an IRQ was handled, false otherwise.
// Called by the emulator before each instruction fetch.
val checkAndHandleIRQ : unit -> bool
function checkAndHandleIRQ() = {
    if IRQPending == 0b1 & I == 0b0 then {
        // Hardware IRQ sequence:
        // 1. Push PC (high byte first, then low)
        push16(PC);
        // 2. Push P with B flag clear (bit 4 = 0 for hardware IRQ)
        push(getP() & 0xEF);
        // 3. Set interrupt disable flag
        I = 0b1;
        // 4. Load PC from IRQ vector
        PC = readMem16(IrqVector);
        NextPC = PC;
        // Note: IRQPending is NOT cleared here - it's level-triggered.
        // The device must clear it (e.g., by acknowledging the interrupt).
        true
    } else {
        false
    }
}

// Check for and handle pending NMI.
// NMI is edge-triggered and cannot be masked.
// Returns true if an NMI was handled, false otherwise.
val checkAndHandleNMI : unit -> bool
function checkAndHandleNMI() = {
    if NMIPending == 0b1 then {
        // NMI sequence (same as IRQ but uses NMI vector)
        push16(PC);
        push(getP() & 0xEF);  // B flag clear
        I = 0b1;
        PC = readMem16(NmiVector);
        NextPC = PC;
        // NMI is edge-triggered, clear the pending flag
        NMIPending = 0b0;
        true
    } else {
        false
    }
}

// =============================================================================
// Instruction AST
// =============================================================================

// The scattered union collects all instruction clauses
scattered union instruction

// Scattered function for execution
val execute : instruction -> ExecutionResult
scattered function execute

// Note: We don't use encdec mapping for 6502 because variable-length
// instructions don't fit SAIL's fixed-width mapping model well.
// TableGen already handles instruction encoding - we only need
// the execute semantics for the emulator.

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


function clause execute LDA_Immediate(imm) = { A = imm; setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_ZeroPage(addr) = { A = readMem(0x00 @ addr); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_ZeroPageX(addr) = { A = readMem(0x00 @ (addr + X)); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_Absolute(addr) = { A = readMem(addr); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_AbsoluteX(addr) = { A = readMem(addr + (0x00 @ X)); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_AbsoluteY(addr) = { A = readMem(addr + (0x00 @ Y)); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_IndexedIndirect(addr) = { let ea = readMem16(0x00 @ (addr + X)); A = readMem(ea); setNZ(A); RETIRE_SUCCESS }
function clause execute LDA_IndirectIndexed(addr) = { let ea = readMem16(0x00 @ addr) + (0x00 @ Y); A = readMem(ea); setNZ(A); RETIRE_SUCCESS }

// =============================================================================
// LDX - Load X Register
// =============================================================================

union clause instruction = LDX_Immediate : bits(8)
union clause instruction = LDX_ZeroPage : bits(8)
union clause instruction = LDX_ZeroPageY : bits(8)
union clause instruction = LDX_Absolute : bits(16)
union clause instruction = LDX_AbsoluteY : bits(16)


function clause execute LDX_Immediate(imm) = { X = imm; setNZ(X); RETIRE_SUCCESS }
function clause execute LDX_ZeroPage(addr) = { X = readMem(0x00 @ addr); setNZ(X); RETIRE_SUCCESS }
function clause execute LDX_ZeroPageY(addr) = { X = readMem(0x00 @ (addr + Y)); setNZ(X); RETIRE_SUCCESS }
function clause execute LDX_Absolute(addr) = { X = readMem(addr); setNZ(X); RETIRE_SUCCESS }
function clause execute LDX_AbsoluteY(addr) = { X = readMem(addr + (0x00 @ Y)); setNZ(X); RETIRE_SUCCESS }

// =============================================================================
// LDY - Load Y Register
// =============================================================================

union clause instruction = LDY_Immediate : bits(8)
union clause instruction = LDY_ZeroPage : bits(8)
union clause instruction = LDY_ZeroPageX : bits(8)
union clause instruction = LDY_Absolute : bits(16)
union clause instruction = LDY_AbsoluteX : bits(16)


function clause execute LDY_Immediate(imm) = { Y = imm; setNZ(Y); RETIRE_SUCCESS }
function clause execute LDY_ZeroPage(addr) = { Y = readMem(0x00 @ addr); setNZ(Y); RETIRE_SUCCESS }
function clause execute LDY_ZeroPageX(addr) = { Y = readMem(0x00 @ (addr + X)); setNZ(Y); RETIRE_SUCCESS }
function clause execute LDY_Absolute(addr) = { Y = readMem(addr); setNZ(Y); RETIRE_SUCCESS }
function clause execute LDY_AbsoluteX(addr) = { Y = readMem(addr + (0x00 @ X)); setNZ(Y); RETIRE_SUCCESS }

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


function clause execute STA_ZeroPage(addr) = { writeMem(0x00 @ addr, A); RETIRE_SUCCESS }
function clause execute STA_ZeroPageX(addr) = { writeMem(0x00 @ (addr + X), A); RETIRE_SUCCESS }
function clause execute STA_Absolute(addr) = { writeMem(addr, A); RETIRE_SUCCESS }
function clause execute STA_AbsoluteX(addr) = { writeMem(addr + (0x00 @ X), A); RETIRE_SUCCESS }
function clause execute STA_AbsoluteY(addr) = { writeMem(addr + (0x00 @ Y), A); RETIRE_SUCCESS }
function clause execute STA_IndexedIndirect(addr) = { let ea = readMem16(0x00 @ (addr + X)); writeMem(ea, A); RETIRE_SUCCESS }
function clause execute STA_IndirectIndexed(addr) = { let ea = readMem16(0x00 @ addr) + (0x00 @ Y); writeMem(ea, A); RETIRE_SUCCESS }

// =============================================================================
// STX - Store X Register
// =============================================================================

union clause instruction = STX_ZeroPage : bits(8)
union clause instruction = STX_ZeroPageY : bits(8)
union clause instruction = STX_Absolute : bits(16)


function clause execute STX_ZeroPage(addr) = { writeMem(0x00 @ addr, X); RETIRE_SUCCESS }
function clause execute STX_ZeroPageY(addr) = { writeMem(0x00 @ (addr + Y), X); RETIRE_SUCCESS }
function clause execute STX_Absolute(addr) = { writeMem(addr, X); RETIRE_SUCCESS }

// =============================================================================
// STY - Store Y Register
// =============================================================================

union clause instruction = STY_ZeroPage : bits(8)
union clause instruction = STY_ZeroPageX : bits(8)
union clause instruction = STY_Absolute : bits(16)


function clause execute STY_ZeroPage(addr) = { writeMem(0x00 @ addr, Y); RETIRE_SUCCESS }
function clause execute STY_ZeroPageX(addr) = { writeMem(0x00 @ (addr + X), Y); RETIRE_SUCCESS }
function clause execute STY_Absolute(addr) = { writeMem(addr, Y); RETIRE_SUCCESS }

// =============================================================================
// ADC - Add with Carry
// =============================================================================

val doADC : bits(8) -> unit
function doADC(value) = {
    if D == 0b1 then {
        // BCD (Decimal) mode
        // Add low nibbles with carry
        let lo : bits(8) = (0x0 @ A[3..0]) + (0x0 @ value[3..0]) + (0b0000000 @ C);
        // If low nibble > 9, add 6 to correct
        let lo_adj : bits(8) = if lo >_u 0x09 then lo + 0x06 else lo;
        // Carry from low to high nibble
        let lo_carry : bits(8) = if lo_adj >_u 0x0F then 0x01 else 0x00;
        // Add high nibbles with carry from low
        let hi : bits(8) = (0x0 @ A[7..4]) + (0x0 @ value[7..4]) + lo_carry;
        // If high nibble > 9, add 6 to correct
        let hi_adj : bits(8) = if hi >_u 0x09 then hi + 0x06 else hi;
        // Set carry if high nibble overflowed
        C = if hi_adj >_u 0x0F then 0b1 else 0b0;
        // Combine nibbles
        A = hi_adj[3..0] @ lo_adj[3..0];
        // N and Z are set based on the result (like binary mode)
        setNZ(A);
        // V is undefined in decimal mode on NMOS 6502, leave unchanged
    } else {
        // Binary mode
        let carry_in : bits(9) = 0x00 @ C;
        let sum : bits(9) = (0b0 @ A) + (0b0 @ value) + carry_in;
        let result : bits(8) = sum[7..0];
        // Overflow: sign of result differs from both operands
        V = (A[7..7] ^ result[7..7]) & (value[7..7] ^ result[7..7]);
        C = sum[8..8];
        A = result;
        setNZ(A)
    }
}

union clause instruction = ADC_Immediate : bits(8)
union clause instruction = ADC_ZeroPage : bits(8)
union clause instruction = ADC_ZeroPageX : bits(8)
union clause instruction = ADC_Absolute : bits(16)
union clause instruction = ADC_AbsoluteX : bits(16)
union clause instruction = ADC_AbsoluteY : bits(16)
union clause instruction = ADC_IndexedIndirect : bits(8)
union clause instruction = ADC_IndirectIndexed : bits(8)


function clause execute ADC_Immediate(imm) = { doADC(imm); RETIRE_SUCCESS }
function clause execute ADC_ZeroPage(addr) = { doADC(readMem(0x00 @ addr)); RETIRE_SUCCESS }
function clause execute ADC_ZeroPageX(addr) = { doADC(readMem(0x00 @ (addr + X))); RETIRE_SUCCESS }
function clause execute ADC_Absolute(addr) = { doADC(readMem(addr)); RETIRE_SUCCESS }
function clause execute ADC_AbsoluteX(addr) = { doADC(readMem(addr + (0x00 @ X))); RETIRE_SUCCESS }
function clause execute ADC_AbsoluteY(addr) = { doADC(readMem(addr + (0x00 @ Y))); RETIRE_SUCCESS }
function clause execute ADC_IndexedIndirect(addr) = { let ea = readMem16(0x00 @ (addr + X)); doADC(readMem(ea)); RETIRE_SUCCESS }
function clause execute ADC_IndirectIndexed(addr) = { let ea = readMem16(0x00 @ addr) + (0x00 @ Y); doADC(readMem(ea)); RETIRE_SUCCESS }

// =============================================================================
// SBC - Subtract with Carry
// =============================================================================

val doSBC : bits(8) -> unit
function doSBC(value) = {
    if D == 0b1 then {
        // BCD (Decimal) mode
        // Subtract low nibbles with borrow (C=1 means no borrow)
        let lo : bits(8) = (0x0 @ A[3..0]) - (0x0 @ value[3..0]) - (0b0000000 @ (C ^ 0b1));
        // If low nibble borrowed (bit 4 set), subtract 6 to correct
        let lo_borrow : bits(1) = lo[4..4];
        let lo_adj : bits(8) = if lo_borrow == 0b1 then lo - 0x06 else lo;
        // Subtract high nibbles with borrow from low
        let hi : bits(8) = (0x0 @ A[7..4]) - (0x0 @ value[7..4]) - (0b0000000 @ lo_borrow);
        // If high nibble borrowed, subtract 6 to correct
        let hi_borrow : bits(1) = hi[4..4];
        let hi_adj : bits(8) = if hi_borrow == 0b1 then hi - 0x06 else hi;
        // Set carry if no borrow occurred
        C = hi_borrow ^ 0b1;
        // Combine nibbles
        A = hi_adj[3..0] @ lo_adj[3..0];
        // N and Z are set based on the result
        setNZ(A);
        // V is undefined in decimal mode on NMOS 6502, leave unchanged
    } else {
        // Binary mode - SBC is ADC with inverted operand
        let borrow_in : bits(9) = 0x00 @ C;
        let inv_value : bits(8) = value ^ 0xFF;
        let diff : bits(9) = (0b0 @ A) + (0b0 @ inv_value) + borrow_in;
        let result : bits(8) = diff[7..0];
        V = (A[7..7] ^ result[7..7]) & (inv_value[7..7] ^ result[7..7]);
        C = diff[8..8];
        A = result;
        setNZ(A)
    }
}

union clause instruction = SBC_Immediate : bits(8)
union clause instruction = SBC_ZeroPage : bits(8)
union clause instruction = SBC_ZeroPageX : bits(8)
union clause instruction = SBC_Absolute : bits(16)
union clause instruction = SBC_AbsoluteX : bits(16)
union clause instruction = SBC_AbsoluteY : bits(16)
union clause instruction = SBC_IndexedIndirect : bits(8)
union clause instruction = SBC_IndirectIndexed : bits(8)


function clause execute SBC_Immediate(imm) = { doSBC(imm); RETIRE_SUCCESS }
function clause execute SBC_ZeroPage(addr) = { doSBC(readMem(0x00 @ addr)); RETIRE_SUCCESS }
function clause execute SBC_ZeroPageX(addr) = { doSBC(readMem(0x00 @ (addr + X))); RETIRE_SUCCESS }
function clause execute SBC_Absolute(addr) = { doSBC(readMem(addr)); RETIRE_SUCCESS }
function clause execute SBC_AbsoluteX(addr) = { doSBC(readMem(addr + (0x00 @ X))); RETIRE_SUCCESS }
function clause execute SBC_AbsoluteY(addr) = { doSBC(readMem(addr + (0x00 @ Y))); RETIRE_SUCCESS }
function clause execute SBC_IndexedIndirect(addr) = { let ea = readMem16(0x00 @ (addr + X)); doSBC(readMem(ea)); RETIRE_SUCCESS }
function clause execute SBC_IndirectIndexed(addr) = { let ea = readMem16(0x00 @ addr) + (0x00 @ Y); doSBC(readMem(ea)); RETIRE_SUCCESS }

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


function clause execute AND_Immediate(imm) = { A = A & imm; setNZ(A); RETIRE_SUCCESS }
function clause execute AND_ZeroPage(addr) = { A = A & readMem(0x00 @ addr); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_ZeroPageX(addr) = { A = A & readMem(0x00 @ (addr + X)); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_Absolute(addr) = { A = A & readMem(addr); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_AbsoluteX(addr) = { A = A & readMem(addr + (0x00 @ X)); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_AbsoluteY(addr) = { A = A & readMem(addr + (0x00 @ Y)); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_IndexedIndirect(addr) = { let ea = readMem16(0x00 @ (addr + X)); A = A & readMem(ea); setNZ(A); RETIRE_SUCCESS }
function clause execute AND_IndirectIndexed(addr) = { let ea = readMem16(0x00 @ addr) + (0x00 @ Y); A = A & readMem(ea); setNZ(A); RETIRE_SUCCESS }

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


function clause execute ORA_Immediate(imm) = { A = A | imm; setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_ZeroPage(addr) = { A = A | readMem(0x00 @ addr); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_ZeroPageX(addr) = { A = A | readMem(0x00 @ (addr + X)); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_Absolute(addr) = { A = A | readMem(addr); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_AbsoluteX(addr) = { A = A | readMem(addr + (0x00 @ X)); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_AbsoluteY(addr) = { A = A | readMem(addr + (0x00 @ Y)); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_IndexedIndirect(addr) = { let ea = readMem16(0x00 @ (addr + X)); A = A | readMem(ea); setNZ(A); RETIRE_SUCCESS }
function clause execute ORA_IndirectIndexed(addr) = { let ea = readMem16(0x00 @ addr) + (0x00 @ Y); A = A | readMem(ea); setNZ(A); RETIRE_SUCCESS }

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


function clause execute EOR_Immediate(imm) = { A = A ^ imm; setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_ZeroPage(addr) = { A = A ^ readMem(0x00 @ addr); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_ZeroPageX(addr) = { A = A ^ readMem(0x00 @ (addr + X)); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_Absolute(addr) = { A = A ^ readMem(addr); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_AbsoluteX(addr) = { A = A ^ readMem(addr + (0x00 @ X)); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_AbsoluteY(addr) = { A = A ^ readMem(addr + (0x00 @ Y)); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_IndexedIndirect(addr) = { let ea = readMem16(0x00 @ (addr + X)); A = A ^ readMem(ea); setNZ(A); RETIRE_SUCCESS }
function clause execute EOR_IndirectIndexed(addr) = { let ea = readMem16(0x00 @ addr) + (0x00 @ Y); A = A ^ readMem(ea); setNZ(A); RETIRE_SUCCESS }

// =============================================================================
// BIT - Bit Test
// =============================================================================

union clause instruction = BIT_ZeroPage : bits(8)
union clause instruction = BIT_Absolute : bits(16)


val doBIT : bits(8) -> unit
function doBIT(value) = {
    N = value[7..7];
    V = value[6..6];
    Z = if (A & value) == 0x00 then 0b1 else 0b0
}

function clause execute BIT_ZeroPage(addr) = { doBIT(readMem(0x00 @ addr)); RETIRE_SUCCESS }
function clause execute BIT_Absolute(addr) = { doBIT(readMem(addr)); RETIRE_SUCCESS }

// =============================================================================
// CMP - Compare Accumulator
// =============================================================================

val doCMP : (bits(8), bits(8)) -> unit
function doCMP(reg, value) = {
    let result : bits(9) = (0b0 @ reg) - (0b0 @ value);
    N = result[7..7];
    Z = if result[7..0] == 0x00 then 0b1 else 0b0;
    C = if reg >=_u value then 0b1 else 0b0
}

union clause instruction = CMP_Immediate : bits(8)
union clause instruction = CMP_ZeroPage : bits(8)
union clause instruction = CMP_ZeroPageX : bits(8)
union clause instruction = CMP_Absolute : bits(16)
union clause instruction = CMP_AbsoluteX : bits(16)
union clause instruction = CMP_AbsoluteY : bits(16)
union clause instruction = CMP_IndexedIndirect : bits(8)
union clause instruction = CMP_IndirectIndexed : bits(8)


function clause execute CMP_Immediate(imm) = { doCMP(A, imm); RETIRE_SUCCESS }
function clause execute CMP_ZeroPage(addr) = { doCMP(A, readMem(0x00 @ addr)); RETIRE_SUCCESS }
function clause execute CMP_ZeroPageX(addr) = { doCMP(A, readMem(0x00 @ (addr + X))); RETIRE_SUCCESS }
function clause execute CMP_Absolute(addr) = { doCMP(A, readMem(addr)); RETIRE_SUCCESS }
function clause execute CMP_AbsoluteX(addr) = { doCMP(A, readMem(addr + (0x00 @ X))); RETIRE_SUCCESS }
function clause execute CMP_AbsoluteY(addr) = { doCMP(A, readMem(addr + (0x00 @ Y))); RETIRE_SUCCESS }
function clause execute CMP_IndexedIndirect(addr) = { let ea = readMem16(0x00 @ (addr + X)); doCMP(A, readMem(ea)); RETIRE_SUCCESS }
function clause execute CMP_IndirectIndexed(addr) = { let ea = readMem16(0x00 @ addr) + (0x00 @ Y); doCMP(A, readMem(ea)); RETIRE_SUCCESS }

// =============================================================================
// CPX - Compare X Register
// =============================================================================

union clause instruction = CPX_Immediate : bits(8)
union clause instruction = CPX_ZeroPage : bits(8)
union clause instruction = CPX_Absolute : bits(16)


function clause execute CPX_Immediate(imm) = { doCMP(X, imm); RETIRE_SUCCESS }
function clause execute CPX_ZeroPage(addr) = { doCMP(X, readMem(0x00 @ addr)); RETIRE_SUCCESS }
function clause execute CPX_Absolute(addr) = { doCMP(X, readMem(addr)); RETIRE_SUCCESS }

// =============================================================================
// CPY - Compare Y Register
// =============================================================================

union clause instruction = CPY_Immediate : bits(8)
union clause instruction = CPY_ZeroPage : bits(8)
union clause instruction = CPY_Absolute : bits(16)


function clause execute CPY_Immediate(imm) = { doCMP(Y, imm); RETIRE_SUCCESS }
function clause execute CPY_ZeroPage(addr) = { doCMP(Y, readMem(0x00 @ addr)); RETIRE_SUCCESS }
function clause execute CPY_Absolute(addr) = { doCMP(Y, readMem(addr)); RETIRE_SUCCESS }

// =============================================================================
// INC - Increment Memory
// =============================================================================

union clause instruction = INC_ZeroPage : bits(8)
union clause instruction = INC_ZeroPageX : bits(8)
union clause instruction = INC_Absolute : bits(16)
union clause instruction = INC_AbsoluteX : bits(16)


val doIncMem : bits(16) -> unit
function doIncMem(ea) = {
    let result = readMem(ea) + 1;
    writeMem(ea, result);
    setNZ(result)
}

function clause execute INC_ZeroPage(addr) = { doIncMem(0x00 @ addr); RETIRE_SUCCESS }
function clause execute INC_ZeroPageX(addr) = { doIncMem(0x00 @ (addr + X)); RETIRE_SUCCESS }
function clause execute INC_Absolute(addr) = { doIncMem(addr); RETIRE_SUCCESS }
function clause execute INC_AbsoluteX(addr) = { doIncMem(addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// DEC - Decrement Memory
// =============================================================================

union clause instruction = DEC_ZeroPage : bits(8)
union clause instruction = DEC_ZeroPageX : bits(8)
union clause instruction = DEC_Absolute : bits(16)
union clause instruction = DEC_AbsoluteX : bits(16)


val doDecMem : bits(16) -> unit
function doDecMem(ea) = {
    let result = readMem(ea) - 1;
    writeMem(ea, result);
    setNZ(result)
}

function clause execute DEC_ZeroPage(addr) = { doDecMem(0x00 @ addr); RETIRE_SUCCESS }
function clause execute DEC_ZeroPageX(addr) = { doDecMem(0x00 @ (addr + X)); RETIRE_SUCCESS }
function clause execute DEC_Absolute(addr) = { doDecMem(addr); RETIRE_SUCCESS }
function clause execute DEC_AbsoluteX(addr) = { doDecMem(addr + (0x00 @ X)); RETIRE_SUCCESS }

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


val doASLMem : bits(16) -> unit
function doASLMem(ea) = {
    let value = readMem(ea);
    C = value[7..7];
    let result = value << 1;
    writeMem(ea, result);
    setNZ(result)
}

function clause execute ASL_Accumulator() = { C = A[7..7]; A = A << 1; setNZ(A); RETIRE_SUCCESS }
function clause execute ASL_ZeroPage(addr) = { doASLMem(0x00 @ addr); RETIRE_SUCCESS }
function clause execute ASL_ZeroPageX(addr) = { doASLMem(0x00 @ (addr + X)); RETIRE_SUCCESS }
function clause execute ASL_Absolute(addr) = { doASLMem(addr); RETIRE_SUCCESS }
function clause execute ASL_AbsoluteX(addr) = { doASLMem(addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// LSR - Logical Shift Right
// =============================================================================

union clause instruction = LSR_Accumulator : unit
union clause instruction = LSR_ZeroPage : bits(8)
union clause instruction = LSR_ZeroPageX : bits(8)
union clause instruction = LSR_Absolute : bits(16)
union clause instruction = LSR_AbsoluteX : bits(16)


val doLSRMem : bits(16) -> unit
function doLSRMem(ea) = {
    let value = readMem(ea);
    C = value[0..0];
    let result = value >> 1;
    writeMem(ea, result);
    setNZ(result)
}

function clause execute LSR_Accumulator() = { C = A[0..0]; A = A >> 1; setNZ(A); RETIRE_SUCCESS }
function clause execute LSR_ZeroPage(addr) = { doLSRMem(0x00 @ addr); RETIRE_SUCCESS }
function clause execute LSR_ZeroPageX(addr) = { doLSRMem(0x00 @ (addr + X)); RETIRE_SUCCESS }
function clause execute LSR_Absolute(addr) = { doLSRMem(addr); RETIRE_SUCCESS }
function clause execute LSR_AbsoluteX(addr) = { doLSRMem(addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// ROL - Rotate Left
// =============================================================================

union clause instruction = ROL_Accumulator : unit
union clause instruction = ROL_ZeroPage : bits(8)
union clause instruction = ROL_ZeroPageX : bits(8)
union clause instruction = ROL_Absolute : bits(16)
union clause instruction = ROL_AbsoluteX : bits(16)


val doROLMem : bits(16) -> unit
function doROLMem(ea) = {
    let value = readMem(ea);
    let old_carry = C;
    C = value[7..7];
    let result : bits(8) = (value[6..0] @ old_carry);
    writeMem(ea, result);
    setNZ(result)
}

function clause execute ROL_Accumulator() = { let old_c = C; C = A[7..7]; A = A[6..0] @ old_c; setNZ(A); RETIRE_SUCCESS }
function clause execute ROL_ZeroPage(addr) = { doROLMem(0x00 @ addr); RETIRE_SUCCESS }
function clause execute ROL_ZeroPageX(addr) = { doROLMem(0x00 @ (addr + X)); RETIRE_SUCCESS }
function clause execute ROL_Absolute(addr) = { doROLMem(addr); RETIRE_SUCCESS }
function clause execute ROL_AbsoluteX(addr) = { doROLMem(addr + (0x00 @ X)); RETIRE_SUCCESS }

// =============================================================================
// ROR - Rotate Right
// =============================================================================

union clause instruction = ROR_Accumulator : unit
union clause instruction = ROR_ZeroPage : bits(8)
union clause instruction = ROR_ZeroPageX : bits(8)
union clause instruction = ROR_Absolute : bits(16)
union clause instruction = ROR_AbsoluteX : bits(16)


val doRORMem : bits(16) -> unit
function doRORMem(ea) = {
    let value = readMem(ea);
    let old_carry = C;
    C = value[0..0];
    let result : bits(8) = (old_carry @ value[7..1]);
    writeMem(ea, result);
    setNZ(result)
}

function clause execute ROR_Accumulator() = { let old_c = C; C = A[0..0]; A = old_c @ A[7..1]; setNZ(A); RETIRE_SUCCESS }
function clause execute ROR_ZeroPage(addr) = { doRORMem(0x00 @ addr); RETIRE_SUCCESS }
function clause execute ROR_ZeroPageX(addr) = { doRORMem(0x00 @ (addr + X)); RETIRE_SUCCESS }
function clause execute ROR_Absolute(addr) = { doRORMem(addr); RETIRE_SUCCESS }
function clause execute ROR_AbsoluteX(addr) = { doRORMem(addr + (0x00 @ X)); RETIRE_SUCCESS }

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


val doBranch : (bool, bits(8)) -> ExecutionResult
function doBranch(cond, offset) = {
    if cond then setNextPC(NextPC + sign_extend(offset));
    RETIRE_SUCCESS
}

function clause execute BPL_Relative(offset) = doBranch(N == 0b0, offset)
function clause execute BMI_Relative(offset) = doBranch(N == 0b1, offset)
function clause execute BVC_Relative(offset) = doBranch(V == 0b0, offset)
function clause execute BVS_Relative(offset) = doBranch(V == 0b1, offset)
function clause execute BCC_Relative(offset) = doBranch(C == 0b0, offset)
function clause execute BCS_Relative(offset) = doBranch(C == 0b1, offset)
function clause execute BNE_Relative(offset) = doBranch(Z == 0b0, offset)
function clause execute BEQ_Relative(offset) = doBranch(Z == 0b1, offset)

// =============================================================================
// Jump Instructions
// =============================================================================

union clause instruction = JMP_Absolute : bits(16)
union clause instruction = JMP_Indirect16 : bits(16)


function clause execute JMP_Absolute(addr) = { setNextPC(addr); RETIRE_SUCCESS }
// Note: 6502 bug - JMP ($xxFF) wraps within page
function clause execute JMP_Indirect16(addr) = {
    let lo = readMem(addr);
    // Bug: high byte comes from same page, not addr+1
    let hi_addr : bits(16) = addr[15..8] @ ((addr[7..0]) + 1);
    let hi = readMem(hi_addr);
    setNextPC(hi @ lo);
    RETIRE_SUCCESS
}

// =============================================================================
// Subroutine Instructions
// =============================================================================

union clause instruction = JSR_Absolute : bits(16)
union clause instruction = RTS_Implied : unit
union clause instruction = RTI_Implied : unit


function clause execute JSR_Absolute(addr) = {
    // Push return address - 1 (pointing to last byte of JSR instruction)
    // NextPC points to instruction after JSR, so NextPC - 1 is last byte of JSR
    let ret = NextPC - 1;
    push16(ret);
    setNextPC(addr);
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

// =============================================================================
// Main entry point (required by SAIL)
// =============================================================================

val main : unit -> unit
function main() = ()
