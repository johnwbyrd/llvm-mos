//===-- MOSEmulator.td - MOS Emulation Semantics -----------*- tablegen -*-===//
//
// Part of LLVM-MOS, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines emulation semantics for MOS instructions. These definitions
// are used to generate an emulator that can execute MOS machine code.
//
// Design:
//   - EmulatorState defines CPU registers and helper methods
//   - EmulatorAddrMode defines how to resolve operand addresses/values
//   - EmulatorBehavior defines what an instruction does (composable)
//   - EmulatorInst ties an existing MOSInst to addressing + behavior
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Emulator State
//===----------------------------------------------------------------------===//

// The EmulatorState class defines the CPU state that the emulator maintains.
// The Helpers field contains C++ code for common operations.

class EmulatorState {
  code Helpers = [{}];
}

def MOS6502State : EmulatorState {
  // The generated emulator will have these as member variables.
  // This is documentation - the actual struct is hand-written for now.
  //
  // Registers:
  //   uint8_t A, X, Y, S;
  //   uint16_t PC;
  //   bool C, Z, I, D, B, V, N;
  //   uint64_t Cycles;
  //   bool Halted;
  //
  // Memory interface:
  //   std::function<uint8_t(uint16_t)> read;
  //   std::function<void(uint16_t, uint8_t)> write;

  let Helpers = [{
    void setNZ(uint8_t val) {
      N = (val >> 7) & 1;
      Z = val == 0;
    }
    void push(uint8_t val) { write(0x100 + S--, val); }
    uint8_t pull() { return read(0x100 + ++S); }
    void push16(uint16_t val) { push(val >> 8); push(val & 0xFF); }
    uint16_t pull16() { uint8_t lo = pull(); return lo | (pull() << 8); }
    bool pageCrossed(uint16_t a, uint16_t b) {
      return (a & 0xFF00) != (b & 0xFF00);
    }
  }];
}

//===----------------------------------------------------------------------===//
// Emulator Addressing Modes
//===----------------------------------------------------------------------===//

// EmulatorAddrMode defines how to compute effective addresses and read values.
// Different addressing modes produce different combinations of:
//   - $EA: effective address (for memory operations)
//   - $Value: the value at that address (for read operations)
//   - $Base: base address before indexing (for page-cross detection)
//
// The operand bytes are available as Inst.getOperand(N).getImm().

class EmulatorAddrMode {
  // C++ expression to compute effective address (empty if N/A)
  // Referenced as $EA in behavior code
  code EA = [{}];
  // C++ expression to read the value (empty if N/A)
  // Referenced as $Value in behavior code
  code Value = [{}];
  // C++ expression for base address before indexing (for page cross)
  // Referenced as $Base in behavior code
  code Base = [{}];
}

// Implied: no operand
def EmuAM_Implied : EmulatorAddrMode;

// Accumulator: operates on A register
def EmuAM_Accumulator : EmulatorAddrMode {
  let Value = [{ A }];
}

// Immediate: operand is the value itself
def EmuAM_Immediate : EmulatorAddrMode {
  let Value = [{ (uint8_t)Inst.getOperand(0).getImm() }];
}

// Zero Page: 8-bit address in zero page
def EmuAM_ZeroPage : EmulatorAddrMode {
  let EA = [{ (uint16_t)Inst.getOperand(0).getImm() }];
  let Value = [{ read($EA) }];
}

// Zero Page,X: zero page address + X
def EmuAM_ZeroPageX : EmulatorAddrMode {
  let EA = [{ (uint8_t)(Inst.getOperand(0).getImm() + X) }];
  let Value = [{ read($EA) }];
}

// Zero Page,Y: zero page address + Y
def EmuAM_ZeroPageY : EmulatorAddrMode {
  let EA = [{ (uint8_t)(Inst.getOperand(0).getImm() + Y) }];
  let Value = [{ read($EA) }];
}

// Absolute: 16-bit address
def EmuAM_Absolute : EmulatorAddrMode {
  let EA = [{ (uint16_t)Inst.getOperand(0).getImm() }];
  let Value = [{ read($EA) }];
}

// Absolute,X: 16-bit address + X (page cross possible)
def EmuAM_AbsoluteX : EmulatorAddrMode {
  let Base = [{ (uint16_t)Inst.getOperand(0).getImm() }];
  let EA = [{ (uint16_t)($Base + X) }];
  let Value = [{ read($EA) }];
}

// Absolute,Y: 16-bit address + Y (page cross possible)
def EmuAM_AbsoluteY : EmulatorAddrMode {
  let Base = [{ (uint16_t)Inst.getOperand(0).getImm() }];
  let EA = [{ (uint16_t)($Base + Y) }];
  let Value = [{ read($EA) }];
}

// Indexed Indirect (d,X): (zp + X) points to 16-bit address
def EmuAM_IndexedIndirect : EmulatorAddrMode {
  let EA = [{
    uint8_t zp = (uint8_t)(Inst.getOperand(0).getImm() + X);
    (uint16_t)(read(zp) | (read((uint8_t)(zp + 1)) << 8))
  }];
  let Value = [{ read($EA) }];
}

// Indirect Indexed (d),Y: (zp) points to address, then + Y (page cross possible)
def EmuAM_IndirectIndexed : EmulatorAddrMode {
  let Base = [{
    uint8_t zp = Inst.getOperand(0).getImm();
    (uint16_t)(read(zp) | (read((uint8_t)(zp + 1)) << 8))
  }];
  let EA = [{ (uint16_t)($Base + Y) }];
  let Value = [{ read($EA) }];
}

// Relative: PC + signed offset (for branches)
def EmuAM_Relative : EmulatorAddrMode {
  let EA = [{ (uint16_t)(PC + 2 + (int8_t)Inst.getOperand(0).getImm()) }];
}

// Indirect16: 16-bit pointer to 16-bit address (JMP indirect)
def EmuAM_Indirect16 : EmulatorAddrMode {
  let EA = [{
    // 6502 page-wrap bug: JMP ($xxFF) fetches high byte from $xx00
    uint16_t ptr = Inst.getOperand(0).getImm();
    (uint16_t)(read(ptr) | (read((ptr & 0xFF00) | ((ptr + 1) & 0xFF)) << 8))
  }];
}

//===----------------------------------------------------------------------===//
// 65816 16-bit Addressing Modes
//===----------------------------------------------------------------------===//

// Immediate16: 16-bit operand value
def EmuAM_Immediate16 : EmulatorAddrMode {
  let Value = [{ (uint16_t)Inst.getOperand(0).getImm() }];
}

// ZeroPage with 16-bit read
def EmuAM_ZeroPage16 : EmulatorAddrMode {
  let EA = [{ (uint16_t)Inst.getOperand(0).getImm() }];
  let Value = [{ read16($EA) }];
}

// Absolute with 16-bit read
def EmuAM_Absolute16 : EmulatorAddrMode {
  let EA = [{ (uint16_t)Inst.getOperand(0).getImm() }];
  let Value = [{ read16($EA) }];
}

//===----------------------------------------------------------------------===//
// Emulator Behaviors
//===----------------------------------------------------------------------===//

// EmulatorBehavior defines what an instruction does. Behaviors are composable:
// you can inherit from multiple behavior classes to build up complex semantics.
//
// The Emulate field contains C++ code that will be executed. It can reference:
//   - $EA: effective address (from addressing mode)
//   - $Value: value read (from addressing mode)
//   - $Base: base address (from addressing mode)
//   - State variables: A, X, Y, S, PC, C, Z, I, D, B, V, N, Cycles
//   - Helpers: setNZ(), push(), pull(), push16(), pull16(), read(), write()

class EmulatorBehavior {
  code Emulate = [{}];
}

//-----------------------------------------------------------------------------
// Load/Store behaviors
//-----------------------------------------------------------------------------

class LoadReg<string reg> : EmulatorBehavior {
  let Emulate = [{
    }] # reg # [{ = $Value;
    setNZ(}] # reg # [{);
  }];
}

class StoreReg<string reg> : EmulatorBehavior {
  let Emulate = [{
    write($EA, }] # reg # [{);
  }];
}

//-----------------------------------------------------------------------------
// Transfer behaviors
//-----------------------------------------------------------------------------

class TransferReg<string src, string dst, bit affectsFlags = 1> : EmulatorBehavior {
  let Emulate = !if(affectsFlags,
    dst # [{ = }] # src # [{; setNZ(}] # dst # [{); }],
    dst # [{ = }] # src # [{; }]
  );
}

//-----------------------------------------------------------------------------
// Arithmetic behaviors
//-----------------------------------------------------------------------------

class BinaryLogic<string op> : EmulatorBehavior {
  let Emulate = [{
    A = A }] # op # [{ $Value;
    setNZ(A);
  }];
}

def EmuAND : BinaryLogic<"&">;
def EmuORA : BinaryLogic<"|">;
def EmuEOR : BinaryLogic<"^">;

def EmuADC : EmulatorBehavior {
  let Emulate = [{
    uint16_t sum = A + $Value + C;
    C = sum > 0xFF;
    V = ~(A ^ $Value) & (A ^ sum) & 0x80;
    A = sum & 0xFF;
    setNZ(A);
  }];
}

def EmuSBC : EmulatorBehavior {
  let Emulate = [{
    uint16_t diff = A - $Value - !C;
    C = diff <= 0xFF;  // No borrow
    V = (A ^ $Value) & (A ^ diff) & 0x80;
    A = diff & 0xFF;
    setNZ(A);
  }];
}

//-----------------------------------------------------------------------------
// Compare behaviors
//-----------------------------------------------------------------------------

class Compare<string reg> : EmulatorBehavior {
  let Emulate = [{
    uint8_t diff = }] # reg # [{ - $Value;
    C = }] # reg # [{ >= $Value;
    setNZ(diff);
  }];
}

def EmuCMP : Compare<"A">;
def EmuCPX : Compare<"X">;
def EmuCPY : Compare<"Y">;

//-----------------------------------------------------------------------------
// Increment/Decrement behaviors
//-----------------------------------------------------------------------------

class IncDecReg<string reg, string op> : EmulatorBehavior {
  let Emulate = reg # op # [{; setNZ(}] # reg # [{); }];
}

class IncDecMem<string op> : EmulatorBehavior {
  let Emulate = [{
    uint8_t val = read($EA);
    val}] # op # [{;
    setNZ(val);
    write($EA, val);
  }];
}

//-----------------------------------------------------------------------------
// Shift/Rotate behaviors
//-----------------------------------------------------------------------------

def EmuASL_A : EmulatorBehavior {
  let Emulate = [{
    C = A >> 7;
    A <<= 1;
    setNZ(A);
  }];
}

def EmuLSR_A : EmulatorBehavior {
  let Emulate = [{
    C = A & 1;
    A >>= 1;
    setNZ(A);
  }];
}

def EmuROL_A : EmulatorBehavior {
  let Emulate = [{
    uint8_t c = C;
    C = A >> 7;
    A = (A << 1) | c;
    setNZ(A);
  }];
}

def EmuROR_A : EmulatorBehavior {
  let Emulate = [{
    uint8_t c = C;
    C = A & 1;
    A = (A >> 1) | (c << 7);
    setNZ(A);
  }];
}

def EmuASL_Mem : EmulatorBehavior {
  let Emulate = [{
    uint8_t val = read($EA);
    C = val >> 7;
    val <<= 1;
    setNZ(val);
    write($EA, val);
  }];
}

def EmuLSR_Mem : EmulatorBehavior {
  let Emulate = [{
    uint8_t val = read($EA);
    C = val & 1;
    val >>= 1;
    setNZ(val);
    write($EA, val);
  }];
}

def EmuROL_Mem : EmulatorBehavior {
  let Emulate = [{
    uint8_t val = read($EA);
    uint8_t c = C;
    C = val >> 7;
    val = (val << 1) | c;
    setNZ(val);
    write($EA, val);
  }];
}

def EmuROR_Mem : EmulatorBehavior {
  let Emulate = [{
    uint8_t val = read($EA);
    uint8_t c = C;
    C = val & 1;
    val = (val >> 1) | (c << 7);
    setNZ(val);
    write($EA, val);
  }];
}

//-----------------------------------------------------------------------------
// Branch behaviors
//-----------------------------------------------------------------------------

class BranchIf<string flag, bit negate = 0> : EmulatorBehavior {
  let Emulate = [{
    bool cond = }] # !if(negate, "!", "") # flag # [{;
    if (cond) {
      Cycles += 1;  // Taken penalty
      if (pageCrossed(PC + 2, $EA)) Cycles += 1;  // Page cross penalty
      PC = $EA;
      return;  // Don't advance PC normally
    }
  }];
}

//-----------------------------------------------------------------------------
// Stack behaviors
//-----------------------------------------------------------------------------

def EmuPHA : EmulatorBehavior { let Emulate = [{ push(A); }]; }
def EmuPHP : EmulatorBehavior { let Emulate = [{ push(getP() | 0x30); }]; }
def EmuPLA : EmulatorBehavior { let Emulate = [{ A = pull(); setNZ(A); }]; }
def EmuPLP : EmulatorBehavior { let Emulate = [{ setP(pull()); }]; }

//-----------------------------------------------------------------------------
// Jump/Call/Return behaviors
//-----------------------------------------------------------------------------

def EmuJMP : EmulatorBehavior {
  let Emulate = [{ PC = $EA; return; }];
}

def EmuJSR : EmulatorBehavior {
  let Emulate = [{
    push16(PC + 2);  // Push return address - 1
    PC = $EA;
    return;
  }];
}

def EmuRTS : EmulatorBehavior {
  let Emulate = [{
    PC = pull16() + 1;
    return;
  }];
}

def EmuRTI : EmulatorBehavior {
  let Emulate = [{
    setP(pull());
    PC = pull16();
    return;
  }];
}

def EmuBRK : EmulatorBehavior {
  let Emulate = [{
    push16(PC + 2);
    push(getP() | 0x30);  // B flag set
    I = true;
    PC = read(0xFFFE) | (read(0xFFFF) << 8);
    return;
  }];
}

//-----------------------------------------------------------------------------
// Flag behaviors
//-----------------------------------------------------------------------------

def EmuCLC : EmulatorBehavior { let Emulate = [{ C = false; }]; }
def EmuSEC : EmulatorBehavior { let Emulate = [{ C = true; }]; }
def EmuCLI : EmulatorBehavior { let Emulate = [{ I = false; }]; }
def EmuSEI : EmulatorBehavior { let Emulate = [{ I = true; }]; }
def EmuCLD : EmulatorBehavior { let Emulate = [{ D = false; }]; }
def EmuSED : EmulatorBehavior { let Emulate = [{ D = true; }]; }
def EmuCLV : EmulatorBehavior { let Emulate = [{ V = false; }]; }

//-----------------------------------------------------------------------------
// BIT behavior
//-----------------------------------------------------------------------------

def EmuBIT : EmulatorBehavior {
  let Emulate = [{
    uint8_t val = $Value;
    Z = (A & val) == 0;
    N = val >> 7;
    V = (val >> 6) & 1;
  }];
}

//-----------------------------------------------------------------------------
// NOP behavior
//-----------------------------------------------------------------------------

def EmuNOP : EmulatorBehavior {
  let Emulate = [{ /* nothing */ }];
}

//===----------------------------------------------------------------------===//
// Emulator Instruction Definitions
//===----------------------------------------------------------------------===//

// EmulatorInst ties together:
//   - A reference to the existing MOSInst (for opcode, cycle count)
//   - An addressing mode (how to get $EA, $Value)
//   - A behavior (what the instruction does)
//
// The TableGen backend will generate a switch statement that:
//   1. Matches on Inst.getOpcode()
//   2. Computes $EA, $Base, $Value from the addressing mode
//   3. Executes the behavior code
//   4. Adds base cycles (from TSFlags) + page cross penalty if applicable
//   5. Advances PC by instruction size

// EmulatorInst inherits from both addressing mode and behavior so their
// fields (Emulate, EA, Value, Base, etc.) become available on this record.
class EmulatorInst<Instruction inst, EmulatorAddrMode am, EmulatorBehavior beh>
    : EmulatorAddrMode, EmulatorBehavior {
  Instruction Inst = inst;
  // Inherit fields from addressing mode
  let EA = am.EA;
  let Value = am.Value;
  let Base = am.Base;
  // Inherit Emulate from behavior
  let Emulate = beh.Emulate;
}

//===----------------------------------------------------------------------===//
// Instruction Emulation Definitions
//===----------------------------------------------------------------------===//
// Only defining a representative subset for now to validate the design.

// Load A
def : EmulatorInst<LDA_Immediate, EmuAM_Immediate, LoadReg<"A">>;
def : EmulatorInst<LDA_ZeroPage, EmuAM_ZeroPage, LoadReg<"A">>;
def : EmulatorInst<LDA_ZeroPageX, EmuAM_ZeroPageX, LoadReg<"A">>;
def : EmulatorInst<LDA_Absolute, EmuAM_Absolute, LoadReg<"A">>;
def : EmulatorInst<LDA_AbsoluteX, EmuAM_AbsoluteX, LoadReg<"A">>;
def : EmulatorInst<LDA_AbsoluteY, EmuAM_AbsoluteY, LoadReg<"A">>;
def : EmulatorInst<LDA_IndexedIndirect, EmuAM_IndexedIndirect, LoadReg<"A">>;
def : EmulatorInst<LDA_IndirectIndexed, EmuAM_IndirectIndexed, LoadReg<"A">>;

// Load X
def : EmulatorInst<LDX_Immediate, EmuAM_Immediate, LoadReg<"X">>;
def : EmulatorInst<LDX_ZeroPage, EmuAM_ZeroPage, LoadReg<"X">>;
def : EmulatorInst<LDX_ZeroPageY, EmuAM_ZeroPageY, LoadReg<"X">>;
def : EmulatorInst<LDX_Absolute, EmuAM_Absolute, LoadReg<"X">>;
def : EmulatorInst<LDX_AbsoluteY, EmuAM_AbsoluteY, LoadReg<"X">>;

// Load Y
def : EmulatorInst<LDY_Immediate, EmuAM_Immediate, LoadReg<"Y">>;
def : EmulatorInst<LDY_ZeroPage, EmuAM_ZeroPage, LoadReg<"Y">>;
def : EmulatorInst<LDY_ZeroPageX, EmuAM_ZeroPageX, LoadReg<"Y">>;
def : EmulatorInst<LDY_Absolute, EmuAM_Absolute, LoadReg<"Y">>;
def : EmulatorInst<LDY_AbsoluteX, EmuAM_AbsoluteX, LoadReg<"Y">>;

// Store A
def : EmulatorInst<STA_ZeroPage, EmuAM_ZeroPage, StoreReg<"A">>;
def : EmulatorInst<STA_ZeroPageX, EmuAM_ZeroPageX, StoreReg<"A">>;
def : EmulatorInst<STA_Absolute, EmuAM_Absolute, StoreReg<"A">>;
def : EmulatorInst<STA_AbsoluteX, EmuAM_AbsoluteX, StoreReg<"A">>;
def : EmulatorInst<STA_AbsoluteY, EmuAM_AbsoluteY, StoreReg<"A">>;
def : EmulatorInst<STA_IndexedIndirect, EmuAM_IndexedIndirect, StoreReg<"A">>;
def : EmulatorInst<STA_IndirectIndexed, EmuAM_IndirectIndexed, StoreReg<"A">>;

// Store X
def : EmulatorInst<STX_ZeroPage, EmuAM_ZeroPage, StoreReg<"X">>;
def : EmulatorInst<STX_ZeroPageY, EmuAM_ZeroPageY, StoreReg<"X">>;
def : EmulatorInst<STX_Absolute, EmuAM_Absolute, StoreReg<"X">>;

// Store Y
def : EmulatorInst<STY_ZeroPage, EmuAM_ZeroPage, StoreReg<"Y">>;
def : EmulatorInst<STY_ZeroPageX, EmuAM_ZeroPageX, StoreReg<"Y">>;
def : EmulatorInst<STY_Absolute, EmuAM_Absolute, StoreReg<"Y">>;

// Transfers
def : EmulatorInst<TAX_Implied, EmuAM_Implied, TransferReg<"A", "X">>;
def : EmulatorInst<TAY_Implied, EmuAM_Implied, TransferReg<"A", "Y">>;
def : EmulatorInst<TXA_Implied, EmuAM_Implied, TransferReg<"X", "A">>;
def : EmulatorInst<TYA_Implied, EmuAM_Implied, TransferReg<"Y", "A">>;
def : EmulatorInst<TSX_Implied, EmuAM_Implied, TransferReg<"S", "X">>;
def : EmulatorInst<TXS_Implied, EmuAM_Implied, TransferReg<"X", "S", 0>>;

// Logic - AND
def : EmulatorInst<AND_Immediate, EmuAM_Immediate, EmuAND>;
def : EmulatorInst<AND_ZeroPage, EmuAM_ZeroPage, EmuAND>;
def : EmulatorInst<AND_ZeroPageX, EmuAM_ZeroPageX, EmuAND>;
def : EmulatorInst<AND_Absolute, EmuAM_Absolute, EmuAND>;
def : EmulatorInst<AND_AbsoluteX, EmuAM_AbsoluteX, EmuAND>;
def : EmulatorInst<AND_AbsoluteY, EmuAM_AbsoluteY, EmuAND>;
def : EmulatorInst<AND_IndexedIndirect, EmuAM_IndexedIndirect, EmuAND>;
def : EmulatorInst<AND_IndirectIndexed, EmuAM_IndirectIndexed, EmuAND>;

// Logic - ORA
def : EmulatorInst<ORA_Immediate, EmuAM_Immediate, EmuORA>;
def : EmulatorInst<ORA_ZeroPage, EmuAM_ZeroPage, EmuORA>;
def : EmulatorInst<ORA_ZeroPageX, EmuAM_ZeroPageX, EmuORA>;
def : EmulatorInst<ORA_Absolute, EmuAM_Absolute, EmuORA>;
def : EmulatorInst<ORA_AbsoluteX, EmuAM_AbsoluteX, EmuORA>;
def : EmulatorInst<ORA_AbsoluteY, EmuAM_AbsoluteY, EmuORA>;
def : EmulatorInst<ORA_IndexedIndirect, EmuAM_IndexedIndirect, EmuORA>;
def : EmulatorInst<ORA_IndirectIndexed, EmuAM_IndirectIndexed, EmuORA>;

// Logic - EOR
def : EmulatorInst<EOR_Immediate, EmuAM_Immediate, EmuEOR>;
def : EmulatorInst<EOR_ZeroPage, EmuAM_ZeroPage, EmuEOR>;
def : EmulatorInst<EOR_ZeroPageX, EmuAM_ZeroPageX, EmuEOR>;
def : EmulatorInst<EOR_Absolute, EmuAM_Absolute, EmuEOR>;
def : EmulatorInst<EOR_AbsoluteX, EmuAM_AbsoluteX, EmuEOR>;
def : EmulatorInst<EOR_AbsoluteY, EmuAM_AbsoluteY, EmuEOR>;
def : EmulatorInst<EOR_IndexedIndirect, EmuAM_IndexedIndirect, EmuEOR>;
def : EmulatorInst<EOR_IndirectIndexed, EmuAM_IndirectIndexed, EmuEOR>;

// Arithmetic - ADC
def : EmulatorInst<ADC_Immediate, EmuAM_Immediate, EmuADC>;
def : EmulatorInst<ADC_ZeroPage, EmuAM_ZeroPage, EmuADC>;
def : EmulatorInst<ADC_ZeroPageX, EmuAM_ZeroPageX, EmuADC>;
def : EmulatorInst<ADC_Absolute, EmuAM_Absolute, EmuADC>;
def : EmulatorInst<ADC_AbsoluteX, EmuAM_AbsoluteX, EmuADC>;
def : EmulatorInst<ADC_AbsoluteY, EmuAM_AbsoluteY, EmuADC>;
def : EmulatorInst<ADC_IndexedIndirect, EmuAM_IndexedIndirect, EmuADC>;
def : EmulatorInst<ADC_IndirectIndexed, EmuAM_IndirectIndexed, EmuADC>;

// Arithmetic - SBC
def : EmulatorInst<SBC_Immediate, EmuAM_Immediate, EmuSBC>;
def : EmulatorInst<SBC_ZeroPage, EmuAM_ZeroPage, EmuSBC>;
def : EmulatorInst<SBC_ZeroPageX, EmuAM_ZeroPageX, EmuSBC>;
def : EmulatorInst<SBC_Absolute, EmuAM_Absolute, EmuSBC>;
def : EmulatorInst<SBC_AbsoluteX, EmuAM_AbsoluteX, EmuSBC>;
def : EmulatorInst<SBC_AbsoluteY, EmuAM_AbsoluteY, EmuSBC>;
def : EmulatorInst<SBC_IndexedIndirect, EmuAM_IndexedIndirect, EmuSBC>;
def : EmulatorInst<SBC_IndirectIndexed, EmuAM_IndirectIndexed, EmuSBC>;

// Compare - CMP
def : EmulatorInst<CMP_Immediate, EmuAM_Immediate, EmuCMP>;
def : EmulatorInst<CMP_ZeroPage, EmuAM_ZeroPage, EmuCMP>;
def : EmulatorInst<CMP_ZeroPageX, EmuAM_ZeroPageX, EmuCMP>;
def : EmulatorInst<CMP_Absolute, EmuAM_Absolute, EmuCMP>;
def : EmulatorInst<CMP_AbsoluteX, EmuAM_AbsoluteX, EmuCMP>;
def : EmulatorInst<CMP_AbsoluteY, EmuAM_AbsoluteY, EmuCMP>;
def : EmulatorInst<CMP_IndexedIndirect, EmuAM_IndexedIndirect, EmuCMP>;
def : EmulatorInst<CMP_IndirectIndexed, EmuAM_IndirectIndexed, EmuCMP>;

// Compare - CPX/CPY
def : EmulatorInst<CPX_Immediate, EmuAM_Immediate, EmuCPX>;
def : EmulatorInst<CPX_ZeroPage, EmuAM_ZeroPage, EmuCPX>;
def : EmulatorInst<CPX_Absolute, EmuAM_Absolute, EmuCPX>;
def : EmulatorInst<CPY_Immediate, EmuAM_Immediate, EmuCPY>;
def : EmulatorInst<CPY_ZeroPage, EmuAM_ZeroPage, EmuCPY>;
def : EmulatorInst<CPY_Absolute, EmuAM_Absolute, EmuCPY>;

// Increment/Decrement registers
def : EmulatorInst<INX_Implied, EmuAM_Implied, IncDecReg<"X", "++">>;
def : EmulatorInst<INY_Implied, EmuAM_Implied, IncDecReg<"Y", "++">>;
def : EmulatorInst<DEX_Implied, EmuAM_Implied, IncDecReg<"X", "--">>;
def : EmulatorInst<DEY_Implied, EmuAM_Implied, IncDecReg<"Y", "--">>;

// Increment/Decrement memory
def : EmulatorInst<INC_ZeroPage, EmuAM_ZeroPage, IncDecMem<"++">>;
def : EmulatorInst<INC_ZeroPageX, EmuAM_ZeroPageX, IncDecMem<"++">>;
def : EmulatorInst<INC_Absolute, EmuAM_Absolute, IncDecMem<"++">>;
def : EmulatorInst<INC_AbsoluteX, EmuAM_AbsoluteX, IncDecMem<"++">>;
def : EmulatorInst<DEC_ZeroPage, EmuAM_ZeroPage, IncDecMem<"--">>;
def : EmulatorInst<DEC_ZeroPageX, EmuAM_ZeroPageX, IncDecMem<"--">>;
def : EmulatorInst<DEC_Absolute, EmuAM_Absolute, IncDecMem<"--">>;
def : EmulatorInst<DEC_AbsoluteX, EmuAM_AbsoluteX, IncDecMem<"--">>;

// Shifts - Accumulator
def : EmulatorInst<ASL_Accumulator, EmuAM_Accumulator, EmuASL_A>;
def : EmulatorInst<LSR_Accumulator, EmuAM_Accumulator, EmuLSR_A>;
def : EmulatorInst<ROL_Accumulator, EmuAM_Accumulator, EmuROL_A>;
def : EmulatorInst<ROR_Accumulator, EmuAM_Accumulator, EmuROR_A>;

// Shifts - Memory
def : EmulatorInst<ASL_ZeroPage, EmuAM_ZeroPage, EmuASL_Mem>;
def : EmulatorInst<ASL_ZeroPageX, EmuAM_ZeroPageX, EmuASL_Mem>;
def : EmulatorInst<ASL_Absolute, EmuAM_Absolute, EmuASL_Mem>;
def : EmulatorInst<ASL_AbsoluteX, EmuAM_AbsoluteX, EmuASL_Mem>;
def : EmulatorInst<LSR_ZeroPage, EmuAM_ZeroPage, EmuLSR_Mem>;
def : EmulatorInst<LSR_ZeroPageX, EmuAM_ZeroPageX, EmuLSR_Mem>;
def : EmulatorInst<LSR_Absolute, EmuAM_Absolute, EmuLSR_Mem>;
def : EmulatorInst<LSR_AbsoluteX, EmuAM_AbsoluteX, EmuLSR_Mem>;
def : EmulatorInst<ROL_ZeroPage, EmuAM_ZeroPage, EmuROL_Mem>;
def : EmulatorInst<ROL_ZeroPageX, EmuAM_ZeroPageX, EmuROL_Mem>;
def : EmulatorInst<ROL_Absolute, EmuAM_Absolute, EmuROL_Mem>;
def : EmulatorInst<ROL_AbsoluteX, EmuAM_AbsoluteX, EmuROL_Mem>;
def : EmulatorInst<ROR_ZeroPage, EmuAM_ZeroPage, EmuROR_Mem>;
def : EmulatorInst<ROR_ZeroPageX, EmuAM_ZeroPageX, EmuROR_Mem>;
def : EmulatorInst<ROR_Absolute, EmuAM_Absolute, EmuROR_Mem>;
def : EmulatorInst<ROR_AbsoluteX, EmuAM_AbsoluteX, EmuROR_Mem>;

// Branches
def : EmulatorInst<BEQ_Relative, EmuAM_Relative, BranchIf<"Z">>;
def : EmulatorInst<BNE_Relative, EmuAM_Relative, BranchIf<"Z", 1>>;
def : EmulatorInst<BCS_Relative, EmuAM_Relative, BranchIf<"C">>;
def : EmulatorInst<BCC_Relative, EmuAM_Relative, BranchIf<"C", 1>>;
def : EmulatorInst<BMI_Relative, EmuAM_Relative, BranchIf<"N">>;
def : EmulatorInst<BPL_Relative, EmuAM_Relative, BranchIf<"N", 1>>;
def : EmulatorInst<BVS_Relative, EmuAM_Relative, BranchIf<"V">>;
def : EmulatorInst<BVC_Relative, EmuAM_Relative, BranchIf<"V", 1>>;

// Stack
def : EmulatorInst<PHA_Implied, EmuAM_Implied, EmuPHA>;
def : EmulatorInst<PHP_Implied, EmuAM_Implied, EmuPHP>;
def : EmulatorInst<PLA_Implied, EmuAM_Implied, EmuPLA>;
def : EmulatorInst<PLP_Implied, EmuAM_Implied, EmuPLP>;

// Jumps/Calls/Returns
def : EmulatorInst<JMP_Absolute, EmuAM_Absolute, EmuJMP>;
def : EmulatorInst<JMP_Indirect16, EmuAM_Indirect16, EmuJMP>;
def : EmulatorInst<JSR_Absolute, EmuAM_Absolute, EmuJSR>;
def : EmulatorInst<RTS_Implied, EmuAM_Implied, EmuRTS>;
def : EmulatorInst<RTI_Implied, EmuAM_Implied, EmuRTI>;
def : EmulatorInst<BRK_Implied, EmuAM_Implied, EmuBRK>;

// Flags
def : EmulatorInst<CLC_Implied, EmuAM_Implied, EmuCLC>;
def : EmulatorInst<SEC_Implied, EmuAM_Implied, EmuSEC>;
def : EmulatorInst<CLI_Implied, EmuAM_Implied, EmuCLI>;
def : EmulatorInst<SEI_Implied, EmuAM_Implied, EmuSEI>;
def : EmulatorInst<CLD_Implied, EmuAM_Implied, EmuCLD>;
def : EmulatorInst<SED_Implied, EmuAM_Implied, EmuSED>;
def : EmulatorInst<CLV_Implied, EmuAM_Implied, EmuCLV>;

// BIT
def : EmulatorInst<BIT_ZeroPage, EmuAM_ZeroPage, EmuBIT>;
def : EmulatorInst<BIT_Absolute, EmuAM_Absolute, EmuBIT>;

// NOP
def : EmulatorInst<NOP_Implied, EmuAM_Implied, EmuNOP>;

//===----------------------------------------------------------------------===//
// 65C02 Instructions
//===----------------------------------------------------------------------===//

// STZ - Store Zero (65C02+)
def EmuSTZ : EmulatorBehavior {
  let Emulate = [{ write($EA, 0); }];
}

def : EmulatorInst<STZ_ZeroPage, EmuAM_ZeroPage, EmuSTZ>;
def : EmulatorInst<STZ_ZeroPageX, EmuAM_ZeroPageX, EmuSTZ>;
def : EmulatorInst<STZ_Absolute, EmuAM_Absolute, EmuSTZ>;
def : EmulatorInst<STZ_AbsoluteX, EmuAM_AbsoluteX, EmuSTZ>;
