// MOS 65xx SAIL Specification - System
// Memory interface, stack operations, and interrupt handling

// =============================================================================
// Memory Interface
// =============================================================================

val readMem : bits(16) -> bits(8)
val writeMem : (bits(16), bits(8)) -> unit

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
