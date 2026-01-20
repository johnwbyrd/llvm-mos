// MOS 65xx SAIL Specification - Model
// Reset, initialization, fetch, decode, and execution step

// =============================================================================
// Reset
// =============================================================================

val reset : unit -> unit
function reset() = {
  // Per 6502 datasheet reset sequence
  A = 0x00;
  X = 0x00;
  Y = 0x00;
  S = 0xFD;  // Stack pointer initialized to $FD after reset

  // Flags: I set, others cleared
  N = 0b0;
  V = 0b0;
  D = 0b0;
  I = 0b1;   // Interrupts disabled on reset
  Z = 0b0;
  C = 0b0;

  // Clear interrupt pending flags
  IRQPending = 0b0;
  NMIPending = 0b0;

  // Load reset vector
  PC = readMem16(ResetVector);
  NextPC = PC
}

// =============================================================================
// Model Initialization
// =============================================================================

val initModel : unit -> unit
function initModel() = {
  reset()
}

// =============================================================================
// Fetch
// =============================================================================

union FetchResult = {
  FR_Ok    : bits(8),    // Opcode byte
  FR_Error : bits(16),   // Faulting address (for future use)
}

val fetch : unit -> FetchResult
function fetch() = {
  let Opcode = readMem(PC);
  fetchCallback(PC, Opcode);
  FR_Ok(Opcode)
}

// =============================================================================
// Decode
// =============================================================================

// Opcode to instruction length (1, 2, or 3 bytes)
val opcodeLength : bits(8) -> int
function opcodeLength(Op) = {
  // 6502 instruction lengths are determined by addressing mode
  // Pattern: xxxyyy10 are 2-byte (zero page) or 3-byte (absolute) based on yyy
  // This encodes the standard 6502 opcode matrix

  match Op {
    // Implied/Accumulator (1 byte)
    0x00 => 2,  // BRK (has padding byte)
    0x08 => 1,  // PHP
    0x18 => 1,  // CLC
    0x28 => 1,  // PLP
    0x38 => 1,  // SEC
    0x48 => 1,  // PHA
    0x58 => 1,  // CLI
    0x68 => 1,  // PLA
    0x78 => 1,  // SEI
    0x88 => 1,  // DEY
    0x8A => 1,  // TXA
    0x98 => 1,  // TYA
    0x9A => 1,  // TXS
    0xA8 => 1,  // TAY
    0xAA => 1,  // TAX
    0xB8 => 1,  // CLV
    0xBA => 1,  // TSX
    0xC8 => 1,  // INY
    0xCA => 1,  // DEX
    0xD8 => 1,  // CLD
    0xE8 => 1,  // INX
    0xEA => 1,  // NOP
    0xF8 => 1,  // SED
    0x40 => 1,  // RTI
    0x60 => 1,  // RTS
    0x0A => 1,  // ASL A
    0x2A => 1,  // ROL A
    0x4A => 1,  // LSR A
    0x6A => 1,  // ROR A

    // Absolute (3 bytes): xxxxx11x pattern and some others
    0x0C => 3,  // (illegal on 6502, but reserve space)
    0x0D => 3,  // ORA abs
    0x0E => 3,  // ASL abs
    0x19 => 3,  // ORA abs,Y
    0x1D => 3,  // ORA abs,X
    0x1E => 3,  // ASL abs,X
    0x20 => 3,  // JSR abs
    0x2C => 3,  // BIT abs
    0x2D => 3,  // AND abs
    0x2E => 3,  // ROL abs
    0x39 => 3,  // AND abs,Y
    0x3D => 3,  // AND abs,X
    0x3E => 3,  // ROL abs,X
    0x4C => 3,  // JMP abs
    0x4D => 3,  // EOR abs
    0x4E => 3,  // LSR abs
    0x59 => 3,  // EOR abs,Y
    0x5D => 3,  // EOR abs,X
    0x5E => 3,  // LSR abs,X
    0x6C => 3,  // JMP (ind)
    0x6D => 3,  // ADC abs
    0x6E => 3,  // ROR abs
    0x79 => 3,  // ADC abs,Y
    0x7D => 3,  // ADC abs,X
    0x7E => 3,  // ROR abs,X
    0x8C => 3,  // STY abs
    0x8D => 3,  // STA abs
    0x8E => 3,  // STX abs
    0x99 => 3,  // STA abs,Y
    0x9D => 3,  // STA abs,X
    0xAC => 3,  // LDY abs
    0xAD => 3,  // LDA abs
    0xAE => 3,  // LDX abs
    0xB9 => 3,  // LDA abs,Y
    0xBC => 3,  // LDY abs,X
    0xBD => 3,  // LDA abs,X
    0xBE => 3,  // LDX abs,Y
    0xCC => 3,  // CPY abs
    0xCD => 3,  // CMP abs
    0xCE => 3,  // DEC abs
    0xD9 => 3,  // CMP abs,Y
    0xDD => 3,  // CMP abs,X
    0xDE => 3,  // DEC abs,X
    0xEC => 3,  // CPX abs
    0xED => 3,  // SBC abs
    0xEE => 3,  // INC abs
    0xF9 => 3,  // SBC abs,Y
    0xFD => 3,  // SBC abs,X
    0xFE => 3,  // INC abs,X

    // Everything else is 2 bytes (immediate, zero page, relative, indexed indirect)
    _ => 2
  }
}

// Decode opcode to instruction
// Returns None for illegal/unimplemented opcodes
val decode : bits(8) -> option(instruction)
function decode(Op) = {
  // Fetch operand bytes based on instruction length
  let Len = opcodeLength(Op);
  let Op1 : bits(8) = if Len >= 2 then readMem(PC + 1) else 0x00;
  let Op2 : bits(8) = if Len >= 3 then readMem(PC + 2) else 0x00;
  let Addr16 : bits(16) = Op2 @ Op1;

  match Op {
    // LDA
    0xA9 => Some(LDA_Immediate(Op1)),
    0xA5 => Some(LDA_ZeroPage(Op1)),
    0xB5 => Some(LDA_ZeroPageX(Op1)),
    0xAD => Some(LDA_Absolute(Addr16)),
    0xBD => Some(LDA_AbsoluteX(Addr16)),
    0xB9 => Some(LDA_AbsoluteY(Addr16)),
    0xA1 => Some(LDA_IndexedIndirect(Op1)),
    0xB1 => Some(LDA_IndirectIndexed(Op1)),

    // LDX
    0xA2 => Some(LDX_Immediate(Op1)),
    0xA6 => Some(LDX_ZeroPage(Op1)),
    0xB6 => Some(LDX_ZeroPageY(Op1)),
    0xAE => Some(LDX_Absolute(Addr16)),
    0xBE => Some(LDX_AbsoluteY(Addr16)),

    // LDY
    0xA0 => Some(LDY_Immediate(Op1)),
    0xA4 => Some(LDY_ZeroPage(Op1)),
    0xB4 => Some(LDY_ZeroPageX(Op1)),
    0xAC => Some(LDY_Absolute(Addr16)),
    0xBC => Some(LDY_AbsoluteX(Addr16)),

    // STA
    0x85 => Some(STA_ZeroPage(Op1)),
    0x95 => Some(STA_ZeroPageX(Op1)),
    0x8D => Some(STA_Absolute(Addr16)),
    0x9D => Some(STA_AbsoluteX(Addr16)),
    0x99 => Some(STA_AbsoluteY(Addr16)),
    0x81 => Some(STA_IndexedIndirect(Op1)),
    0x91 => Some(STA_IndirectIndexed(Op1)),

    // STX
    0x86 => Some(STX_ZeroPage(Op1)),
    0x96 => Some(STX_ZeroPageY(Op1)),
    0x8E => Some(STX_Absolute(Addr16)),

    // STY
    0x84 => Some(STY_ZeroPage(Op1)),
    0x94 => Some(STY_ZeroPageX(Op1)),
    0x8C => Some(STY_Absolute(Addr16)),

    // ADC
    0x69 => Some(ADC_Immediate(Op1)),
    0x65 => Some(ADC_ZeroPage(Op1)),
    0x75 => Some(ADC_ZeroPageX(Op1)),
    0x6D => Some(ADC_Absolute(Addr16)),
    0x7D => Some(ADC_AbsoluteX(Addr16)),
    0x79 => Some(ADC_AbsoluteY(Addr16)),
    0x61 => Some(ADC_IndexedIndirect(Op1)),
    0x71 => Some(ADC_IndirectIndexed(Op1)),

    // SBC
    0xE9 => Some(SBC_Immediate(Op1)),
    0xE5 => Some(SBC_ZeroPage(Op1)),
    0xF5 => Some(SBC_ZeroPageX(Op1)),
    0xED => Some(SBC_Absolute(Addr16)),
    0xFD => Some(SBC_AbsoluteX(Addr16)),
    0xF9 => Some(SBC_AbsoluteY(Addr16)),
    0xE1 => Some(SBC_IndexedIndirect(Op1)),
    0xF1 => Some(SBC_IndirectIndexed(Op1)),

    // AND
    0x29 => Some(AND_Immediate(Op1)),
    0x25 => Some(AND_ZeroPage(Op1)),
    0x35 => Some(AND_ZeroPageX(Op1)),
    0x2D => Some(AND_Absolute(Addr16)),
    0x3D => Some(AND_AbsoluteX(Addr16)),
    0x39 => Some(AND_AbsoluteY(Addr16)),
    0x21 => Some(AND_IndexedIndirect(Op1)),
    0x31 => Some(AND_IndirectIndexed(Op1)),

    // ORA
    0x09 => Some(ORA_Immediate(Op1)),
    0x05 => Some(ORA_ZeroPage(Op1)),
    0x15 => Some(ORA_ZeroPageX(Op1)),
    0x0D => Some(ORA_Absolute(Addr16)),
    0x1D => Some(ORA_AbsoluteX(Addr16)),
    0x19 => Some(ORA_AbsoluteY(Addr16)),
    0x01 => Some(ORA_IndexedIndirect(Op1)),
    0x11 => Some(ORA_IndirectIndexed(Op1)),

    // EOR
    0x49 => Some(EOR_Immediate(Op1)),
    0x45 => Some(EOR_ZeroPage(Op1)),
    0x55 => Some(EOR_ZeroPageX(Op1)),
    0x4D => Some(EOR_Absolute(Addr16)),
    0x5D => Some(EOR_AbsoluteX(Addr16)),
    0x59 => Some(EOR_AbsoluteY(Addr16)),
    0x41 => Some(EOR_IndexedIndirect(Op1)),
    0x51 => Some(EOR_IndirectIndexed(Op1)),

    // BIT
    0x24 => Some(BIT_ZeroPage(Op1)),
    0x2C => Some(BIT_Absolute(Addr16)),

    // CMP
    0xC9 => Some(CMP_Immediate(Op1)),
    0xC5 => Some(CMP_ZeroPage(Op1)),
    0xD5 => Some(CMP_ZeroPageX(Op1)),
    0xCD => Some(CMP_Absolute(Addr16)),
    0xDD => Some(CMP_AbsoluteX(Addr16)),
    0xD9 => Some(CMP_AbsoluteY(Addr16)),
    0xC1 => Some(CMP_IndexedIndirect(Op1)),
    0xD1 => Some(CMP_IndirectIndexed(Op1)),

    // CPX
    0xE0 => Some(CPX_Immediate(Op1)),
    0xE4 => Some(CPX_ZeroPage(Op1)),
    0xEC => Some(CPX_Absolute(Addr16)),

    // CPY
    0xC0 => Some(CPY_Immediate(Op1)),
    0xC4 => Some(CPY_ZeroPage(Op1)),
    0xCC => Some(CPY_Absolute(Addr16)),

    // INC
    0xE6 => Some(INC_ZeroPage(Op1)),
    0xF6 => Some(INC_ZeroPageX(Op1)),
    0xEE => Some(INC_Absolute(Addr16)),
    0xFE => Some(INC_AbsoluteX(Addr16)),

    // DEC
    0xC6 => Some(DEC_ZeroPage(Op1)),
    0xD6 => Some(DEC_ZeroPageX(Op1)),
    0xCE => Some(DEC_Absolute(Addr16)),
    0xDE => Some(DEC_AbsoluteX(Addr16)),

    // INX, INY, DEX, DEY
    0xE8 => Some(INX_Implied()),
    0xC8 => Some(INY_Implied()),
    0xCA => Some(DEX_Implied()),
    0x88 => Some(DEY_Implied()),

    // ASL
    0x0A => Some(ASL_Accumulator()),
    0x06 => Some(ASL_ZeroPage(Op1)),
    0x16 => Some(ASL_ZeroPageX(Op1)),
    0x0E => Some(ASL_Absolute(Addr16)),
    0x1E => Some(ASL_AbsoluteX(Addr16)),

    // LSR
    0x4A => Some(LSR_Accumulator()),
    0x46 => Some(LSR_ZeroPage(Op1)),
    0x56 => Some(LSR_ZeroPageX(Op1)),
    0x4E => Some(LSR_Absolute(Addr16)),
    0x5E => Some(LSR_AbsoluteX(Addr16)),

    // ROL
    0x2A => Some(ROL_Accumulator()),
    0x26 => Some(ROL_ZeroPage(Op1)),
    0x36 => Some(ROL_ZeroPageX(Op1)),
    0x2E => Some(ROL_Absolute(Addr16)),
    0x3E => Some(ROL_AbsoluteX(Addr16)),

    // ROR
    0x6A => Some(ROR_Accumulator()),
    0x66 => Some(ROR_ZeroPage(Op1)),
    0x76 => Some(ROR_ZeroPageX(Op1)),
    0x6E => Some(ROR_Absolute(Addr16)),
    0x7E => Some(ROR_AbsoluteX(Addr16)),

    // Transfer
    0xAA => Some(TAX_Implied()),
    0x8A => Some(TXA_Implied()),
    0xA8 => Some(TAY_Implied()),
    0x98 => Some(TYA_Implied()),
    0xBA => Some(TSX_Implied()),
    0x9A => Some(TXS_Implied()),

    // Stack
    0x48 => Some(PHA_Implied()),
    0x68 => Some(PLA_Implied()),
    0x08 => Some(PHP_Implied()),
    0x28 => Some(PLP_Implied()),

    // Flags
    0x18 => Some(CLC_Implied()),
    0x38 => Some(SEC_Implied()),
    0x58 => Some(CLI_Implied()),
    0x78 => Some(SEI_Implied()),
    0xD8 => Some(CLD_Implied()),
    0xF8 => Some(SED_Implied()),
    0xB8 => Some(CLV_Implied()),

    // Branch
    0x10 => Some(BPL_Relative(Op1)),
    0x30 => Some(BMI_Relative(Op1)),
    0x50 => Some(BVC_Relative(Op1)),
    0x70 => Some(BVS_Relative(Op1)),
    0x90 => Some(BCC_Relative(Op1)),
    0xB0 => Some(BCS_Relative(Op1)),
    0xD0 => Some(BNE_Relative(Op1)),
    0xF0 => Some(BEQ_Relative(Op1)),

    // Jump
    0x4C => Some(JMP_Absolute(Addr16)),
    0x6C => Some(JMP_Indirect16(Addr16)),

    // Subroutine
    0x20 => Some(JSR_Absolute(Addr16)),
    0x60 => Some(RTS_Implied()),
    0x40 => Some(RTI_Implied()),

    // Misc
    0x00 => Some(BRK_Implied()),
    0xEA => Some(NOP_Implied()),

    // Illegal/unimplemented opcode
    _ => None()
  }
}

// =============================================================================
// Execution Step
// =============================================================================

val tryStep : unit -> ExecutionResult
function tryStep() = {
  // Check for pending interrupts first
  if checkAndHandleNMI() then ER_Success()
  else if checkAndHandleIRQ() then ER_Success()
  else {
    // Fetch
    match fetch() {
      FR_Error(Addr) => ER_IllegalOpcode(0x00),
      FR_Ok(Opcode) => {
        // Decode
        let Len = opcodeLength(Opcode);
        NextPC = PC + Len;

        match decode(Opcode) {
          None() => ER_IllegalOpcode(Opcode),
          Some(Instr) => {
            // Execute
            let Result = execute(Instr);
            // Commit PC
            match Result {
              ER_Success() => { tickPC(); Result },
              _ => Result
            }
          }
        }
      }
    }
  }
}

// =============================================================================
// Main Loop
// =============================================================================

val run : unit -> unit
function run() = {
  var Running : bool = true;
  while Running do {
    match tryStep() {
      ER_Success() => (),
      ER_Halt() => Running = false,
      ER_IllegalOpcode(_) => Running = false,
      ER_Break() => ()  // BRK doesn't halt, just vectors
    }
  }
}

// =============================================================================
// Main Entry Point (required by SAIL)
// =============================================================================

val main : unit -> unit
function main() = {
  initModel();
  run()
}

// =============================================================================
// Isla Footprint Entry Point
// =============================================================================
// This function is called by isla-footprint to symbolically execute a single
// instruction. The opcode parameter contains up to 3 bytes (24 bits) of
// instruction data, with the first byte being the opcode.

$ifdef SYMBOLIC

$include <isla.sail>

// sail_end_cycle marks the end of an instruction cycle (for concurrency models)
val sail_end_cycle = impure "cycle_count" : unit -> unit

// Decode opcode bits directly (without reading from memory)
val isla_decode : bits(24) -> option(instruction)
function isla_decode(opcode) = {
  let Op : bits(8) = opcode[7..0];
  let Op1 : bits(8) = opcode[15..8];
  let Op2 : bits(8) = opcode[23..16];
  let Addr16 : bits(16) = Op2 @ Op1;

  match Op {
    // LDA
    0xA9 => Some(LDA_Immediate(Op1)),
    0xA5 => Some(LDA_ZeroPage(Op1)),
    0xB5 => Some(LDA_ZeroPageX(Op1)),
    0xAD => Some(LDA_Absolute(Addr16)),
    0xBD => Some(LDA_AbsoluteX(Addr16)),
    0xB9 => Some(LDA_AbsoluteY(Addr16)),
    0xA1 => Some(LDA_IndexedIndirect(Op1)),
    0xB1 => Some(LDA_IndirectIndexed(Op1)),

    // LDX
    0xA2 => Some(LDX_Immediate(Op1)),
    0xA6 => Some(LDX_ZeroPage(Op1)),
    0xB6 => Some(LDX_ZeroPageY(Op1)),
    0xAE => Some(LDX_Absolute(Addr16)),
    0xBE => Some(LDX_AbsoluteY(Addr16)),

    // LDY
    0xA0 => Some(LDY_Immediate(Op1)),
    0xA4 => Some(LDY_ZeroPage(Op1)),
    0xB4 => Some(LDY_ZeroPageX(Op1)),
    0xAC => Some(LDY_Absolute(Addr16)),
    0xBC => Some(LDY_AbsoluteX(Addr16)),

    // STA
    0x85 => Some(STA_ZeroPage(Op1)),
    0x95 => Some(STA_ZeroPageX(Op1)),
    0x8D => Some(STA_Absolute(Addr16)),
    0x9D => Some(STA_AbsoluteX(Addr16)),
    0x99 => Some(STA_AbsoluteY(Addr16)),
    0x81 => Some(STA_IndexedIndirect(Op1)),
    0x91 => Some(STA_IndirectIndexed(Op1)),

    // STX
    0x86 => Some(STX_ZeroPage(Op1)),
    0x96 => Some(STX_ZeroPageY(Op1)),
    0x8E => Some(STX_Absolute(Addr16)),

    // STY
    0x84 => Some(STY_ZeroPage(Op1)),
    0x94 => Some(STY_ZeroPageX(Op1)),
    0x8C => Some(STY_Absolute(Addr16)),

    // ADC
    0x69 => Some(ADC_Immediate(Op1)),
    0x65 => Some(ADC_ZeroPage(Op1)),
    0x75 => Some(ADC_ZeroPageX(Op1)),
    0x6D => Some(ADC_Absolute(Addr16)),
    0x7D => Some(ADC_AbsoluteX(Addr16)),
    0x79 => Some(ADC_AbsoluteY(Addr16)),
    0x61 => Some(ADC_IndexedIndirect(Op1)),
    0x71 => Some(ADC_IndirectIndexed(Op1)),

    // SBC
    0xE9 => Some(SBC_Immediate(Op1)),
    0xE5 => Some(SBC_ZeroPage(Op1)),
    0xF5 => Some(SBC_ZeroPageX(Op1)),
    0xED => Some(SBC_Absolute(Addr16)),
    0xFD => Some(SBC_AbsoluteX(Addr16)),
    0xF9 => Some(SBC_AbsoluteY(Addr16)),
    0xE1 => Some(SBC_IndexedIndirect(Op1)),
    0xF1 => Some(SBC_IndirectIndexed(Op1)),

    // AND
    0x29 => Some(AND_Immediate(Op1)),
    0x25 => Some(AND_ZeroPage(Op1)),
    0x35 => Some(AND_ZeroPageX(Op1)),
    0x2D => Some(AND_Absolute(Addr16)),
    0x3D => Some(AND_AbsoluteX(Addr16)),
    0x39 => Some(AND_AbsoluteY(Addr16)),
    0x21 => Some(AND_IndexedIndirect(Op1)),
    0x31 => Some(AND_IndirectIndexed(Op1)),

    // ORA
    0x09 => Some(ORA_Immediate(Op1)),
    0x05 => Some(ORA_ZeroPage(Op1)),
    0x15 => Some(ORA_ZeroPageX(Op1)),
    0x0D => Some(ORA_Absolute(Addr16)),
    0x1D => Some(ORA_AbsoluteX(Addr16)),
    0x19 => Some(ORA_AbsoluteY(Addr16)),
    0x01 => Some(ORA_IndexedIndirect(Op1)),
    0x11 => Some(ORA_IndirectIndexed(Op1)),

    // EOR
    0x49 => Some(EOR_Immediate(Op1)),
    0x45 => Some(EOR_ZeroPage(Op1)),
    0x55 => Some(EOR_ZeroPageX(Op1)),
    0x4D => Some(EOR_Absolute(Addr16)),
    0x5D => Some(EOR_AbsoluteX(Addr16)),
    0x59 => Some(EOR_AbsoluteY(Addr16)),
    0x41 => Some(EOR_IndexedIndirect(Op1)),
    0x51 => Some(EOR_IndirectIndexed(Op1)),

    // BIT
    0x24 => Some(BIT_ZeroPage(Op1)),
    0x2C => Some(BIT_Absolute(Addr16)),

    // CMP
    0xC9 => Some(CMP_Immediate(Op1)),
    0xC5 => Some(CMP_ZeroPage(Op1)),
    0xD5 => Some(CMP_ZeroPageX(Op1)),
    0xCD => Some(CMP_Absolute(Addr16)),
    0xDD => Some(CMP_AbsoluteX(Addr16)),
    0xD9 => Some(CMP_AbsoluteY(Addr16)),
    0xC1 => Some(CMP_IndexedIndirect(Op1)),
    0xD1 => Some(CMP_IndirectIndexed(Op1)),

    // CPX
    0xE0 => Some(CPX_Immediate(Op1)),
    0xE4 => Some(CPX_ZeroPage(Op1)),
    0xEC => Some(CPX_Absolute(Addr16)),

    // CPY
    0xC0 => Some(CPY_Immediate(Op1)),
    0xC4 => Some(CPY_ZeroPage(Op1)),
    0xCC => Some(CPY_Absolute(Addr16)),

    // INC
    0xE6 => Some(INC_ZeroPage(Op1)),
    0xF6 => Some(INC_ZeroPageX(Op1)),
    0xEE => Some(INC_Absolute(Addr16)),
    0xFE => Some(INC_AbsoluteX(Addr16)),

    // DEC
    0xC6 => Some(DEC_ZeroPage(Op1)),
    0xD6 => Some(DEC_ZeroPageX(Op1)),
    0xCE => Some(DEC_Absolute(Addr16)),
    0xDE => Some(DEC_AbsoluteX(Addr16)),

    // INX, INY, DEX, DEY
    0xE8 => Some(INX_Implied()),
    0xC8 => Some(INY_Implied()),
    0xCA => Some(DEX_Implied()),
    0x88 => Some(DEY_Implied()),

    // ASL
    0x0A => Some(ASL_Accumulator()),
    0x06 => Some(ASL_ZeroPage(Op1)),
    0x16 => Some(ASL_ZeroPageX(Op1)),
    0x0E => Some(ASL_Absolute(Addr16)),
    0x1E => Some(ASL_AbsoluteX(Addr16)),

    // LSR
    0x4A => Some(LSR_Accumulator()),
    0x46 => Some(LSR_ZeroPage(Op1)),
    0x56 => Some(LSR_ZeroPageX(Op1)),
    0x4E => Some(LSR_Absolute(Addr16)),
    0x5E => Some(LSR_AbsoluteX(Addr16)),

    // ROL
    0x2A => Some(ROL_Accumulator()),
    0x26 => Some(ROL_ZeroPage(Op1)),
    0x36 => Some(ROL_ZeroPageX(Op1)),
    0x2E => Some(ROL_Absolute(Addr16)),
    0x3E => Some(ROL_AbsoluteX(Addr16)),

    // ROR
    0x6A => Some(ROR_Accumulator()),
    0x66 => Some(ROR_ZeroPage(Op1)),
    0x76 => Some(ROR_ZeroPageX(Op1)),
    0x6E => Some(ROR_Absolute(Addr16)),
    0x7E => Some(ROR_AbsoluteX(Addr16)),

    // Transfer
    0xAA => Some(TAX_Implied()),
    0x8A => Some(TXA_Implied()),
    0xA8 => Some(TAY_Implied()),
    0x98 => Some(TYA_Implied()),
    0xBA => Some(TSX_Implied()),
    0x9A => Some(TXS_Implied()),

    // Stack
    0x48 => Some(PHA_Implied()),
    0x68 => Some(PLA_Implied()),
    0x08 => Some(PHP_Implied()),
    0x28 => Some(PLP_Implied()),

    // Flags
    0x18 => Some(CLC_Implied()),
    0x38 => Some(SEC_Implied()),
    0x58 => Some(CLI_Implied()),
    0x78 => Some(SEI_Implied()),
    0xD8 => Some(CLD_Implied()),
    0xF8 => Some(SED_Implied()),
    0xB8 => Some(CLV_Implied()),

    // Branch
    0x10 => Some(BPL_Relative(Op1)),
    0x30 => Some(BMI_Relative(Op1)),
    0x50 => Some(BVC_Relative(Op1)),
    0x70 => Some(BVS_Relative(Op1)),
    0x90 => Some(BCC_Relative(Op1)),
    0xB0 => Some(BCS_Relative(Op1)),
    0xD0 => Some(BNE_Relative(Op1)),
    0xF0 => Some(BEQ_Relative(Op1)),

    // Jump
    0x4C => Some(JMP_Absolute(Addr16)),
    0x6C => Some(JMP_Indirect16(Addr16)),

    // Subroutine
    0x20 => Some(JSR_Absolute(Addr16)),
    0x60 => Some(RTS_Implied()),
    0x40 => Some(RTI_Implied()),

    // Misc
    0x00 => Some(BRK_Implied()),
    0xEA => Some(NOP_Implied()),

    // Illegal opcode
    _ => None()
  }
}

// Main entry point for isla-footprint
// Returns true if instruction executed successfully, false for illegal opcode
val isla_footprint : bits(24) -> bool
function isla_footprint(opcode) = {
  isla_reset_registers();
  sail_end_cycle();

  match isla_decode(opcode) {
    None() => false,
    Some(instr) => {
      let _ = execute(instr);
      true
    }
  }
}

$endif
