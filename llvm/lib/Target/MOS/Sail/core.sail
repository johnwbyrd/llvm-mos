// MOS 65xx SAIL Specification - Core
// Types, extensions, registers, PC access, and callbacks

// =============================================================================
// Extensions
// =============================================================================

enum Extension = {
  Ext_6502,         // Base NMOS 6502 (always enabled)
  Ext_65C02,        // CMOS 65C02 additions (PHX, PHY, STZ, BRA, etc.)
  Ext_R65C02,       // Rockwell additions (BBR, BBS, RMB, SMB)
  Ext_W65C02,       // WDC additions (WAI, STP)
  Ext_65CE02,       // CSG 65CE02 (Z register, 16-bit branches)
  Ext_65816,        // WDC 65816 in emulation mode
  Ext_65816Native,  // WDC 65816 native mode (16-bit)
  Ext_Decimal       // Decimal mode support (can be disabled)
}

// Hardware capability - static configuration
// Returns true if the hardware supports this extension
val hardwareCapability : Extension -> bool
scattered function hardwareCapability

function clause hardwareCapability(Ext_6502) = true      // Always supported
function clause hardwareCapability(Ext_Decimal) = true   // Default: decimal mode supported
function clause hardwareCapability(_) = false            // Other extensions off by default

// Currently enabled - dynamic runtime state
// Returns true if the extension is currently usable
val currentlyEnabled : Extension -> bool
scattered function currentlyEnabled

function clause currentlyEnabled(Ext_6502) = true
// 65816 native mode requires E flag clear (when we add E register)
// function clause currentlyEnabled(Ext_65816Native) = hardwareCapability(Ext_65816) & E == 0b0
function clause currentlyEnabled(Ext) = hardwareCapability(Ext)

// =============================================================================
// Execution Result
// =============================================================================

union ExecutionResult = {
  ER_Success       : unit,
  ER_IllegalOpcode : bits(8),
  ER_Break         : unit,
  ER_Halt          : unit,      // For WDC WAI/STP
}

// Convenience constant for backward compatibility
let RETIRE_SUCCESS : ExecutionResult = ER_Success()

// =============================================================================
// Registers
// =============================================================================

register A : bits(8)       // Accumulator
register X : bits(8)       // X index register
register Y : bits(8)       // Y index register
register S : bits(8)       // Stack pointer
register PC : bits(16)     // Program counter
register NextPC : bits(16) // Next PC (set before execute, may be overridden)

// Status flags (P register bits)
register N : bits(1)       // Negative (bit 7)
register V : bits(1)       // Overflow (bit 6)
register D : bits(1)       // Decimal mode (bit 3)
register I : bits(1)       // Interrupt disable (bit 2)
register Z : bits(1)       // Zero (bit 1)
register C : bits(1)       // Carry (bit 0)

// Interrupt state
register IRQPending : bits(1)  // Hardware IRQ line asserted (level-triggered)
register NMIPending : bits(1)  // Non-maskable interrupt pending (edge-triggered)

// =============================================================================
// PC Access (following sail-riscv pattern)
// =============================================================================

val getArchPC : unit -> bits(16)
function getArchPC() = PC

val getNextPC : unit -> bits(16)
function getNextPC() = NextPC

val setNextPC : bits(16) -> unit
function setNextPC(Pc) = {
  NextPC = Pc
}

val tickPC : unit -> unit
function tickPC() = {
  PC = NextPC
}

// =============================================================================
// Callbacks for External Integration
// =============================================================================

// These are stub implementations. External code (e.g., MAME, debugger)
// can override these via C++ linkage.

val fetchCallback : (bits(16), bits(8)) -> unit
function fetchCallback(_, _) = ()

val memReadCallback : (bits(16), bits(8)) -> unit
function memReadCallback(_, _) = ()

val memWriteCallback : (bits(16), bits(8)) -> unit
function memWriteCallback(_, _) = ()

val regWriteCallback : (string, bits(8)) -> unit
function regWriteCallback(_, _) = ()

val pcWriteCallback : bits(16) -> unit
function pcWriteCallback(_) = ()

val irqCallback : (bool, bits(16)) -> unit  // (IsNmi, VectorAddress)
function irqCallback(_, _) = ()

// =============================================================================
// String Conversion for Debugging (toStr overloads)
// =============================================================================

// Hex formatting helpers
val hexBits1 : bits(1) -> string
function hexBits1(B) = if B == 0b1 then "1" else "0"

val hexBits8 : bits(8) -> string
function hexBits8(B) = {
  let HexChars = "0123456789ABCDEF";
  let Hi = unsigned(B[7..4]);
  let Lo = unsigned(B[3..0]);



























  concat_str(dec_str(Hi), dec_str(Lo))
}

val hexBits16 : bits(16) -> string
function hexBits16(B) = {
  concat_str(hexBits8(B[15..8]), hexBits8(B[7..0]))
}

// Extension to string
val extensionToStr : Extension -> string
function extensionToStr(Ext) = {
  match Ext {
    Ext_6502        => "6502",
    Ext_65C02       => "65C02",
    Ext_R65C02      => "R65C02",
    Ext_W65C02      => "W65C02",
    Ext_65CE02      => "65CE02",
    Ext_65816       => "65816",
    Ext_65816Native => "65816Native",
    Ext_Decimal     => "Decimal"
  }
}

// ExecutionResult to string
val resultToStr : ExecutionResult -> string
function resultToStr(R) = {
  match R {
    ER_Success()       => "Success",
    ER_IllegalOpcode(Op) => concat_str("Illegal opcode: $", hexBits8(Op)),
    ER_Break()         => "BRK",
    ER_Halt()          => "Halt"
  }
}

// Flags to string (like "NV-BDIZC")
val flagsToStr : unit -> string
function flagsToStr() = {
  concat_str(
    concat_str(
      concat_str(
        concat_str(if N == 0b1 then "N" else "n",
                   if V == 0b1 then "V" else "v"),
        concat_str("-",
                   if D == 0b1 then "D" else "d")),
      concat_str(if I == 0b1 then "I" else "i",
                 if Z == 0b1 then "Z" else "z")),
    if C == 0b1 then "C" else "c")
}

// Full register state to string
val regsToStr : unit -> string
function regsToStr() = {
  concat_str(
    concat_str(
      concat_str(
        concat_str("A=", hexBits8(A)),
        concat_str(" X=", hexBits8(X))),
      concat_str(
        concat_str(" Y=", hexBits8(Y)),
        concat_str(" S=", hexBits8(S)))),
    concat_str(
      concat_str(" PC=", hexBits16(PC)),
      concat_str(" P=", flagsToStr())))
}

// Overload toStr for common types
overload toStr = {extensionToStr, resultToStr, flagsToStr, regsToStr}
