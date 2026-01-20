// MOS 65xx SAIL Specification - System
// Memory interface, stack operations, interrupt handling, and cycle tracking

// =============================================================================
// Concurrency Interface (required for Isla symbolic execution)
// =============================================================================

$include <concurrency_interface.sail>

// Address conversion: MOS uses 16-bit addresses, but the concurrency interface
// requires 32 or 64 bit. We zero-extend to 64-bit.
val mos_addr_to_bits64 : bits(16) -> bits(64)
function mos_addr_to_bits64(addr) = sail_zero_extend(addr, 64)

// Instantiate the memory read interface
instantiation sail_mem_read with
    'pa = bits(16),
    'translation_summary = unit,
    'arch_ak = unit,
    'abort = unit,
    pa_bits = mos_addr_to_bits64

// Instantiate the memory write interface
instantiation sail_mem_write with
    'pa = bits(16),
    'translation_summary = unit,
    'arch_ak = unit,
    'abort = unit,
    pa_bits = mos_addr_to_bits64

// =============================================================================
// Cycle Tracking - Class-Based System
// =============================================================================

// Cycle counter - incremented by each instruction
register Cycles : int

// Operation class - determines base cycle count
enum OpClass = {
  OC_Read,   // Load/compare operations
  OC_Write,  // Store operations
  OC_RMW     // Read-modify-write (INC, DEC, ASL, LSR, ROL, ROR on memory)
}

// Addressing mode class
enum AddrMode = {
  AM_Implied,       // No operand (TAX, INX, etc.) - 2 cycles
  AM_Accumulator,   // Operates on A (ASL A, etc.) - 2 cycles
  AM_Immediate,     // #$nn - 2 cycles
  AM_ZeroPage,      // $nn - 3/3/5 cycles (read/write/rmw)
  AM_ZeroPageX,     // $nn,X - 4/4/6 cycles
  AM_ZeroPageY,     // $nn,Y - 4/4/6 cycles
  AM_Absolute,      // $nnnn - 4/4/6 cycles
  AM_AbsoluteX,     // $nnnn,X - 4*/5/7 cycles (*+1 if page cross for read)
  AM_AbsoluteY,     // $nnnn,Y - 4*/5/7 cycles (*+1 if page cross for read)
  AM_IndirectX,     // ($nn,X) - 6/6/- cycles
  AM_IndirectY,     // ($nn),Y - 5*/6/- cycles (*+1 if page cross for read)
  AM_Relative,      // Branches - special handling
  AM_Indirect       // JMP ($nnnn) - special: 5 cycles
}

// Base cycle counts by addressing mode and operation class
// Returns (base_cycles, page_cross_penalty)
// This is a scattered function so variants can override specific cases
val baseCycles : (AddrMode, OpClass) -> (int, int)
scattered function baseCycles

// -----------------------------------------------------------------------------
// NMOS 6502 Base Timings (default)
// -----------------------------------------------------------------------------

// Implied/Accumulator: always 2 cycles, no page cross
function clause baseCycles(AM_Implied, _)     = (2, 0)
function clause baseCycles(AM_Accumulator, _) = (2, 0)

// Immediate: always 2 cycles, no page cross
function clause baseCycles(AM_Immediate, _)   = (2, 0)

// Zero Page: 3/3/5 cycles (read/write/rmw), no page cross possible
function clause baseCycles(AM_ZeroPage, OC_Read)  = (3, 0)
function clause baseCycles(AM_ZeroPage, OC_Write) = (3, 0)
function clause baseCycles(AM_ZeroPage, OC_RMW)   = (5, 0)

// Zero Page,X: 4/4/6 cycles, no page cross (wraps in ZP)
function clause baseCycles(AM_ZeroPageX, OC_Read)  = (4, 0)
function clause baseCycles(AM_ZeroPageX, OC_Write) = (4, 0)
function clause baseCycles(AM_ZeroPageX, OC_RMW)   = (6, 0)

// Zero Page,Y: 4/4/6 cycles, no page cross (wraps in ZP)
function clause baseCycles(AM_ZeroPageY, OC_Read)  = (4, 0)
function clause baseCycles(AM_ZeroPageY, OC_Write) = (4, 0)
function clause baseCycles(AM_ZeroPageY, OC_RMW)   = (6, 0)

// Absolute: 4/4/6 cycles, no page cross
function clause baseCycles(AM_Absolute, OC_Read)  = (4, 0)
function clause baseCycles(AM_Absolute, OC_Write) = (4, 0)
function clause baseCycles(AM_Absolute, OC_RMW)   = (6, 0)

// Absolute,X: read has page cross penalty, write/rmw always take extra cycle
// NOTE: On 65C02, RMW gets page cross optimization - override in variant file
function clause baseCycles(AM_AbsoluteX, OC_Read)  = (4, 1)  // +1 if page cross
function clause baseCycles(AM_AbsoluteX, OC_Write) = (5, 0)  // always 5
function clause baseCycles(AM_AbsoluteX, OC_RMW)   = (7, 0)  // NMOS: always 7

// Absolute,Y: read has page cross penalty, write always takes extra cycle
// (no RMW instructions use abs,Y on 6502)
function clause baseCycles(AM_AbsoluteY, OC_Read)  = (4, 1)  // +1 if page cross
function clause baseCycles(AM_AbsoluteY, OC_Write) = (5, 0)  // always 5
function clause baseCycles(AM_AbsoluteY, OC_RMW)   = (7, 0)  // theoretical

// Indexed Indirect ($nn,X): 6 cycles, no page cross
function clause baseCycles(AM_IndirectX, OC_Read)  = (6, 0)
function clause baseCycles(AM_IndirectX, OC_Write) = (6, 0)
function clause baseCycles(AM_IndirectX, OC_RMW)   = (8, 0)  // theoretical

// Indirect Indexed ($nn),Y: read has page cross penalty
function clause baseCycles(AM_IndirectY, OC_Read)  = (5, 1)  // +1 if page cross
function clause baseCycles(AM_IndirectY, OC_Write) = (6, 0)  // always 6
function clause baseCycles(AM_IndirectY, OC_RMW)   = (8, 0)  // theoretical

// Relative (branches) - base 2, special handling for taken/page cross
function clause baseCycles(AM_Relative, _) = (2, 0)

// Indirect (JMP only) - 5 cycles
function clause baseCycles(AM_Indirect, _) = (5, 0)

// Check if adding an index register to a base address crosses a page boundary
val pageCrossed : (bits(16), bits(8)) -> bool
function pageCrossed(base, index) = {
  let effective = base + (0x00 @ index);
  base[15..8] != effective[15..8]
}

// Calculate cycles for an instruction given its addressing mode and operation class
// For indexed modes that may have page cross penalty, pass the base address and index
val calcCycles : (AddrMode, OpClass) -> int
function calcCycles(mode, op) = {
  let (base, _) = baseCycles(mode, op);
  base
}

// Calculate cycles with page cross check
val calcCyclesIndexed : (AddrMode, OpClass, bits(16), bits(8)) -> int
function calcCyclesIndexed(mode, op, base, index) = {
  let (cycles, penalty) = baseCycles(mode, op);
  if penalty > 0 & pageCrossed(base, index) then
    cycles + penalty
  else
    cycles
}

// Branch cycle calculation: 2 (not taken), +1 (taken), +1 (page cross)
val calcBranchCycles : (bool, bits(16), bits(8)) -> int
function calcBranchCycles(taken, pc, offset) = {
  if taken then {
    // Sign-extend offset and add to PC+2 (after fetching branch instruction)
    let target : bits(16) = (pc + 2) + sign_extend(16, offset);
    let crossed = (pc + 2)[15..8] != target[15..8];
    if crossed then 4 else 3
  } else {
    2
  }
}

// Add cycles to counter
val addCycles : int -> unit
function addCycles(n) = Cycles = Cycles + n

// Reset cycle counter
val resetCycles : unit -> unit
function resetCycles() = Cycles = 0

// Get current cycle count
val getCycles : unit -> int
function getCycles() = Cycles

// =============================================================================
// Special Instruction Cycle Counts (exceptions to the rules)
// =============================================================================

// These instructions don't follow the standard pattern:
//   BRK: 7 cycles
//   JSR: 6 cycles
//   RTS: 6 cycles
//   RTI: 6 cycles
//   JMP abs: 3 cycles (not 4 like other absolute)
//   JMP ind: 5 cycles
//   PHA/PHP: 3 cycles
//   PLA/PLP: 4 cycles

let CYCLES_BRK : int = 7
let CYCLES_JSR : int = 6
let CYCLES_RTS : int = 6
let CYCLES_RTI : int = 6
let CYCLES_JMP_ABS : int = 3
let CYCLES_JMP_IND : int = 5
let CYCLES_PUSH : int = 3   // PHA, PHP
let CYCLES_PULL : int = 4   // PLA, PLP
let CYCLES_IRQ : int = 7    // Hardware interrupt
let CYCLES_NMI : int = 7    // Non-maskable interrupt

// =============================================================================
// Memory Interface
// =============================================================================

// Helper: create a plain read request for MOS
val mos_read_request : bits(16) -> Mem_read_request(1, 64, bits(16), unit, unit)
function mos_read_request(addr) = struct {
    access_kind = AK_explicit(struct { variety = AV_plain, strength = AS_normal }),
    va = None(),
    pa = addr,
    translation = (),
    size = 1,
    tag = false
}

// Helper: create a plain write request for MOS
val mos_write_request : (bits(16), bits(8)) -> Mem_write_request(1, 64, bits(16), unit, unit)
function mos_write_request(addr, data) = struct {
    access_kind = AK_explicit(struct { variety = AV_plain, strength = AS_normal }),
    va = None(),
    pa = addr,
    translation = (),
    size = 1,
    value = Some(data),
    tag = None()
}

// Read a byte from memory
val readMem : bits(16) -> bits(8)
function readMem(addr) = {
    match sail_mem_read(mos_read_request(addr)) {
        Ok((data, _)) => data,
        Err(_) => 0x00  // Return 0 on error (shouldn't happen in normal operation)
    }
}

// Write a byte to memory
val writeMem : (bits(16), bits(8)) -> unit
function writeMem(addr, data) = {
    let _ = sail_mem_write(mos_write_request(addr, data));
    ()
}

// Read 16-bit value (little-endian)
val readMem16 : bits(16) -> bits(16)
function readMem16(Addr) = {
  let Lo = readMem(Addr);
  let Hi = readMem(Addr + 1);
  Hi @ Lo
}

// =============================================================================
// Stack Operations
// =============================================================================

val push : bits(8) -> unit
function push(Value) = {
  writeMem(0x01 @ S, Value);
  S = S - 1
}

val pull : unit -> bits(8)
function pull() = {
  S = S + 1;
  readMem(0x01 @ S)
}

val push16 : bits(16) -> unit
function push16(Value) = {
  push(Value[15..8]);  // High byte first
  push(Value[7..0])    // Low byte second
}

val pull16 : unit -> bits(16)
function pull16() = {
  let Lo = pull();
  let Hi = pull();
  Hi @ Lo
}

// =============================================================================
// Flag Helpers
// =============================================================================

val setNZ : bits(8) -> unit
function setNZ(Result) = {
  N = Result[7..7];
  Z = if Result == 0x00 then 0b1 else 0b0
}

val getP : unit -> bits(8)
function getP() = N @ V @ 0b11 @ D @ I @ Z @ C

val setP : bits(8) -> unit
function setP(P) = {
  N = P[7..7];
  V = P[6..6];
  D = P[3..3];
  I = P[2..2];
  Z = P[1..1];
  C = P[0..0]
}

// =============================================================================
// Interrupt Handling
// =============================================================================

let IrqVector : bits(16) = 0xFFFE
let NmiVector : bits(16) = 0xFFFA
let ResetVector : bits(16) = 0xFFFC

// Check for and handle pending IRQ.
// Returns true if an IRQ was handled.
// IRQ is level-triggered: IRQPending must be cleared by the device.
val checkAndHandleIRQ : unit -> bool
function checkAndHandleIRQ() = {
  if IRQPending == 0b1 & I == 0b0 then {
    addCycles(CYCLES_IRQ);
    irqCallback(false, IrqVector);
    push16(PC);
    push(getP() & 0xEF);  // B flag clear for hardware IRQ
    I = 0b1;
    PC = readMem16(IrqVector);
    NextPC = PC;
    true
  } else {
    false
  }
}

// Check for and handle pending NMI.
// Returns true if an NMI was handled.
// NMI is edge-triggered: NMIPending is cleared after handling.
val checkAndHandleNMI : unit -> bool
function checkAndHandleNMI() = {
  if NMIPending == 0b1 then {
    addCycles(CYCLES_NMI);
    irqCallback(true, NmiVector);
    push16(PC);
    push(getP() & 0xEF);  // B flag clear
    I = 0b1;
    PC = readMem16(NmiVector);
    NextPC = PC;
    NMIPending = 0b0;
    true
  } else {
    false
  }
}
