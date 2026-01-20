// MOS R65C02 SAIL Specification
// Rockwell R65C02 - extends 65C02 with bit manipulation instructions
//
// Inheritance: 6502 (base) -> 65C02 -> R65C02
//
// Include this file AFTER 65c02.sail to enable R65C02 features.
// This file contains EVERYTHING specific to the R65C02 variant:
//   - Hardware capability enable
//   - Cycle timing overrides (if any)
//   - New instructions (RMB, SMB, BBR, BBS)

// =============================================================================
// Hardware Capability
// =============================================================================

function clause hardwareCapability(Ext_R65C02) = true

// =============================================================================
// Cycle Timing
// =============================================================================

// R65C02 has the same base timing as 65C02 (no overrides needed)
// The new bit instructions have fixed cycle counts:
let CYCLES_RMB : int = 5  // RMB0-RMB7: 5 cycles (ZP RMW)
let CYCLES_SMB : int = 5  // SMB0-SMB7: 5 cycles (ZP RMW)
let CYCLES_BBR : int = 5  // BBR0-BBR7: 5 cycles (not taken), 6-7 (taken)
let CYCLES_BBS : int = 5  // BBS0-BBS7: 5 cycles (not taken), 6-7 (taken)

// =============================================================================
// New Instructions: RMB - Reset Memory Bit
// =============================================================================

// RMB0-RMB7: Clear bit N in zero page location
// Encoding: $07, $17, $27, $37, $47, $57, $67, $77

// Helper for RMB operations
val doRMB : (int, bits(8)) -> unit
function doRMB(BitNum, Addr) = {
  let Value = readMem(0x00 @ Addr);
  let Mask : bits(8) = ~(0x01 << BitNum);
  writeMem(0x00 @ Addr, Value & Mask)
}

// union clause instruction = RMB0_ZeroPage : bits(8)
// function clause execute RMB0_ZeroPage(Addr) = { addCycles(CYCLES_RMB); doRMB(0, Addr); RETIRE_SUCCESS }
// ... (RMB1-RMB7 follow same pattern)

// =============================================================================
// New Instructions: SMB - Set Memory Bit
// =============================================================================

// SMB0-SMB7: Set bit N in zero page location
// Encoding: $87, $97, $A7, $B7, $C7, $D7, $E7, $F7

// Helper for SMB operations
val doSMB : (int, bits(8)) -> unit
function doSMB(BitNum, Addr) = {
  let Value = readMem(0x00 @ Addr);
  let Mask : bits(8) = 0x01 << BitNum;
  writeMem(0x00 @ Addr, Value | Mask)
}

// union clause instruction = SMB0_ZeroPage : bits(8)
// function clause execute SMB0_ZeroPage(Addr) = { addCycles(CYCLES_SMB); doSMB(0, Addr); RETIRE_SUCCESS }
// ... (SMB1-SMB7 follow same pattern)

// =============================================================================
// New Instructions: BBR - Branch on Bit Reset
// =============================================================================

// BBR0-BBR7: Branch if bit N in zero page location is clear
// Encoding: $0F, $1F, $2F, $3F, $4F, $5F, $6F, $7F
// 3-byte instruction: opcode, zp address, relative offset

// Helper for BBR operations - returns branch taken status
val doBBR : (int, bits(8)) -> bool
function doBBR(BitNum, Addr) = {
  let Value = readMem(0x00 @ Addr);
  let Mask : bits(8) = 0x01 << BitNum;
  (Value & Mask) == 0x00
}

// union clause instruction = BBR0 : (bits(8), bits(8))  // (ZP addr, offset)
// function clause execute BBR0((Addr, Offset)) = {
//   let taken = doBBR(0, Addr);
//   addCycles(calcBranchCycles(taken, getArchPC(), Offset) + 2);  // +2 for ZP read
//   if taken then {
//     let target = getArchPC() + 3 + sign_extend(16, Offset);
//     setNextPC(target)
//   };
//   RETIRE_SUCCESS
// }
// ... (BBR1-BBR7 follow same pattern)

// =============================================================================
// New Instructions: BBS - Branch on Bit Set
// =============================================================================

// BBS0-BBS7: Branch if bit N in zero page location is set
// Encoding: $8F, $9F, $AF, $BF, $CF, $DF, $EF, $FF
// 3-byte instruction: opcode, zp address, relative offset

// Helper for BBS operations - returns branch taken status
val doBBS : (int, bits(8)) -> bool
function doBBS(BitNum, Addr) = {
  let Value = readMem(0x00 @ Addr);
  let Mask : bits(8) = 0x01 << BitNum;
  (Value & Mask) != 0x00
}

// union clause instruction = BBS0 : (bits(8), bits(8))  // (ZP addr, offset)
// function clause execute BBS0((Addr, Offset)) = {
//   let taken = doBBS(0, Addr);
//   addCycles(calcBranchCycles(taken, getArchPC(), Offset) + 2);  // +2 for ZP read
//   if taken then {
//     let target = getArchPC() + 3 + sign_extend(16, Offset);
//     setNextPC(target)
//   };
//   RETIRE_SUCCESS
// }
// ... (BBS1-BBS7 follow same pattern)
