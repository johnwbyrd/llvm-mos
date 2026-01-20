// MOS W65C02 SAIL Specification
// WDC W65C02 - extends R65C02 with WAI and STP instructions
//
// Inheritance: 6502 (base) -> 65C02 -> R65C02 -> W65C02
//
// Include this file AFTER r65c02.sail to enable W65C02 features.
// This file contains EVERYTHING specific to the W65C02 variant:
//   - Hardware capability enable
//   - Cycle timing overrides (if any)
//   - New instructions (WAI, STP)

// =============================================================================
// Hardware Capability
// =============================================================================

function clause hardwareCapability(Ext_W65C02) = true

// =============================================================================
// Cycle Timing
// =============================================================================

// W65C02 has the same base timing as R65C02 (no overrides needed)
// The new instructions have special timing:
let CYCLES_WAI : int = 3  // WAI: 3 cycles before halt (then waits for interrupt)
let CYCLES_STP : int = 3  // STP: 3 cycles before halt (waits for reset)

// =============================================================================
// New Instructions
// =============================================================================

// -----------------------------------------------------------------------------
// WAI - Wait for Interrupt ($CB)
// -----------------------------------------------------------------------------
// Puts processor in low-power mode until an interrupt occurs.
// After interrupt, execution continues at next instruction (if IRQ)
// or at interrupt vector (if NMI).
// Timing: 3 cycles, then halted until interrupt

union clause instruction = WAI_Implied : unit
function clause execute WAI_Implied() = {
  addCycles(CYCLES_WAI);
  ER_Halt()  // Signal that processor should wait for interrupt
}

// -----------------------------------------------------------------------------
// STP - Stop the Clock ($DB)
// -----------------------------------------------------------------------------
// Stops the processor entirely until a hardware reset.
// Used for ultra-low-power mode.
// Timing: 3 cycles, then halted until reset

union clause instruction = STP_Implied : unit
function clause execute STP_Implied() = {
  addCycles(CYCLES_STP);
  ER_Halt()  // Signal that processor has stopped
}

// =============================================================================
// Bug Fixes in W65C02
// =============================================================================

// The W65C02 fixes several bugs from the NMOS 6502:
//
// 1. JMP ($xxFF) bug - Fixed
//    NMOS: JMP ($12FF) reads low byte from $12FF, high byte from $1200 (wrap bug)
//    W65C02: JMP ($12FF) correctly reads high byte from $1300
//
// 2. Indexed indirect wrap bug - Fixed
//    NMOS: LDA ($FF,X) with X=$01 reads from $00,$01 instead of $100,$101
//    W65C02: Correctly handles the carry
//
// 3. RMW dummy write - Eliminated
//    NMOS: RMW instructions do a dummy write of the original value before
//          writing the modified value (can cause issues with hardware registers)
//    W65C02: Only writes the final value
//
// 4. Decimal mode flags - Fixed
//    NMOS: N, V, Z flags undefined after ADC/SBC in decimal mode
//    W65C02: Flags are correctly set (adds 1 cycle in decimal mode)
//
// These fixes are behavioral and handled by the execute implementations,
// not by cycle timing changes.
